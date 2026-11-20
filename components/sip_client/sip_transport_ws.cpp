#include "sip_transport_ws.h"

#ifdef USE_SIP_CLIENT_WS

#include <mutex>
#ifdef USE_SIP_CLIENT_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sip_net.h"

namespace esphome {
namespace sip_client {

static const char *const TAG = "sip_client.ws";

// Keep NAT/proxy state alive and detect dead links: a ping every 20 s, and the
// connection is aborted when no pong arrives within 45 s (two missed pings).
static const size_t PING_INTERVAL_SEC = 20;
static const int PINGPONG_TIMEOUT_SEC = 45;
static const int RECONNECT_TIMEOUT_MS = 5000;
static const int NETWORK_TIMEOUT_MS = 10000;
// How long send() may block the main loop.
static const uint32_t SEND_TIMEOUT_MS = 500;
static const uint32_t CLOSE_TIMEOUT_MS = 1000;

WsTransport::~WsTransport() {
  this->close();
  if (this->client_ != nullptr) {
    // Destroying joins the client task; it is never called from that task.
    esp_websocket_client_destroy(this->client_);
    this->client_ = nullptr;
  }
}

bool WsTransport::open(const std::string &server, uint16_t port) {
  if (this->client_ == nullptr) {
    this->uri_ = sip_ws_uri(this->cfg_.tls, server, port, this->cfg_.path);

    esp_websocket_client_config_t config = {};
    config.uri = this->uri_.c_str();
    config.subprotocol = "sip";  // RFC 7118 section 3: Sec-WebSocket-Protocol: sip
    config.disable_auto_reconnect = false;
    config.reconnect_timeout_ms = RECONNECT_TIMEOUT_MS;
    config.network_timeout_ms = NETWORK_TIMEOUT_MS;
    config.ping_interval_sec = PING_INTERVAL_SEC;
    config.pingpong_timeout_sec = PINGPONG_TIMEOUT_SEC;
    // Pin the RX buffer; larger frames arrive as several DATA events and are
    // reassembled in WsReassembler.
    config.buffer_size = 2048;
    // TLS handshake + certificate verification needs more stack than plain TCP.
    config.task_stack = this->cfg_.tls ? 8192 : 4096;
    if (this->cfg_.tls) {
      if (!this->cfg_.ca_certificate.empty()) {
        // The TLS layer keeps this pointer (no copy), so cfg_ must outlive the client.
        config.cert_pem = this->cfg_.ca_certificate.c_str();
        config.cert_len = this->cfg_.ca_certificate.size() + 1;  // include the NUL for PEM
      } else if (this->cfg_.verify_ssl) {
#ifdef USE_SIP_CLIENT_CERT_BUNDLE
        config.crt_bundle_attach = esp_crt_bundle_attach;
#endif
      } else {
        // No CA, no bundle: esp-tls skips chain verification (needs
        // CONFIG_ESP_TLS_INSECURE + _SKIP_SERVER_CERT_VERIFY, set from __init__.py).
        config.skip_cert_common_name_check = true;
        ESP_LOGW(TAG, "TLS certificate verification is DISABLED (verify_ssl: false)");
      }
    }

    this->client_ = esp_websocket_client_init(&config);
    if (this->client_ == nullptr) {
      ESP_LOGW(TAG, "Failed to init WebSocket client");
      return false;
    }
    esp_websocket_register_events(this->client_, WEBSOCKET_EVENT_ANY, WsTransport::event_handler_, this);
  }

  {
    std::lock_guard<std::mutex> lock(this->mutex_);
    this->rx_queue_.clear();
    this->reassembler_.reset();
    // A provisional value so Via/Contact are never empty; replaced on connect.
    this->sent_by_ = sip_random_invalid_host([]() { return random_uint32(); });
  }
  esp_err_t err = esp_websocket_client_start(this->client_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_websocket_client_start failed: %d", static_cast<int>(err));
    return false;
  }
  this->started_.store(true, std::memory_order_release);
  ESP_LOGI(TAG, "Connecting to %s", this->uri_.c_str());
  return true;
}

void WsTransport::close() {
  if (this->client_ == nullptr || !this->started_.exchange(false, std::memory_order_acq_rel)) return;
  this->connected_.store(false, std::memory_order_release);
  // Sends a close frame (when connected), then stops the client task.
  esp_websocket_client_close(this->client_, pdMS_TO_TICKS(CLOSE_TIMEOUT_MS));
}

bool WsTransport::send(const std::string &msg) {
  if (!this->connected()) return false;
  int len = static_cast<int>(msg.size());
  int sent = esp_websocket_client_send_text(this->client_, msg.data(), len, pdMS_TO_TICKS(SEND_TIMEOUT_MS));
  return sent == len;
}

bool WsTransport::poll(std::string &out) {
  std::lock_guard<std::mutex> lock(this->mutex_);
  if (this->rx_queue_.empty()) return false;
  out = std::move(this->rx_queue_.front());
  this->rx_queue_.pop_front();
  return true;
}

std::string WsTransport::via_sent_by() const {
  std::lock_guard<std::mutex> lock(this->mutex_);
  return this->sent_by_;
}

std::string WsTransport::local_ip() const { return local_interface_ipv4(); }

void WsTransport::event_handler_(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  auto *self = static_cast<WsTransport *>(handler_args);
  auto *data = static_cast<esp_websocket_event_data_t *>(event_data);
  switch (static_cast<esp_websocket_event_id_t>(event_id)) {
    case WEBSOCKET_EVENT_CONNECTED: {
      {
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->rx_queue_.clear();
        self->reassembler_.reset();
        // New connection, new sent-by; stable until the next drop.
        self->sent_by_ = sip_random_invalid_host([]() { return random_uint32(); });
      }
      self->generation_.fetch_add(1, std::memory_order_acq_rel);
      self->connected_.store(true, std::memory_order_release);
      break;
    }
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
    case WEBSOCKET_EVENT_ERROR: {
      self->connected_.store(false, std::memory_order_release);
      std::lock_guard<std::mutex> lock(self->mutex_);
      self->rx_queue_.clear();  // belongs to a dead connection
      self->reassembler_.reset();
      break;
    }
    case WEBSOCKET_EVENT_DATA:
      if (data != nullptr) self->on_data_(data);
      break;
    default:
      break;
  }
}

void WsTransport::on_data_(const esp_websocket_event_data_t *data) {
  std::string msg;
  std::lock_guard<std::mutex> lock(this->mutex_);
  auto res = this->reassembler_.feed(data->op_code, data->data_ptr, data->data_len, data->payload_len,
                                     data->payload_offset, data->fin, msg);
  if (res == WsReassembler::OVERSIZED) {
    ESP_LOGW(TAG, "Dropping SIP message larger than %u bytes", static_cast<unsigned>(MAX_MESSAGE));
  } else if (res == WsReassembler::MESSAGE) {
    if (this->rx_queue_.size() >= MAX_QUEUE) {
      ESP_LOGW(TAG, "RX queue full, dropping SIP message");
      return;
    }
    this->rx_queue_.push_back(std::move(msg));
  }
}

}  // namespace sip_client
}  // namespace esphome

#endif  // USE_SIP_CLIENT_WS
