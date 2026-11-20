#include "sip_transport_ws.h"

#ifdef USE_SIP_CLIENT_WS

#include <algorithm>
#include <mutex>
#ifdef USE_SIP_CLIENT_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sip_net.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
static const uint32_t SEND_TIMEOUT_MS = 1000;
// A link that stays down this long (growing x2 up to the cap) is restarted.
static const uint32_t WATCHDOG_INITIAL_MS = 3 * 60 * 1000;
static const uint32_t WATCHDOG_MAX_MS = 10 * 60 * 1000;
static const uint32_t RESTART_STUCK_WARN_MS = 60 * 1000;
static const uint32_t CLOSE_TIMEOUT_MS = 1000;

WsTransport::~WsTransport() {
  this->close();
  // The restart helper uses `this` and client_; wait for it (bounded) before
  // destroying.
  for (int i = 0; i < 100 && this->restarting_.load(std::memory_order_acquire); i++)
    vTaskDelay(pdMS_TO_TICKS(100));
  if (this->restarting_.load(std::memory_order_acquire)) {
    // The helper task still dereferences `this` after we return, so this is a
    // use-after-free in that corner case. Acceptable: ESPHome components live
    // for the whole firmware run and are never destroyed.
    ESP_LOGW(TAG, "Restart task still running at shutdown; leaving the WebSocket client alone");
    return;
  }
  if (this->client_ != nullptr) {
    // Destroying joins the client task; it is never called from that task.
    esp_websocket_client_destroy(this->client_);
    this->client_ = nullptr;
  }
}

bool WsTransport::open(const std::string &server, uint16_t port) {
  const bool first_start = this->client_ == nullptr;
  if (first_start) {
    this->uri_ = sip_ws_uri(this->cfg_.tls, server, port, this->cfg_.path);

    esp_websocket_client_config_t config = {};
    config.uri = this->uri_.c_str();
    config.subprotocol = "sip";  // RFC 7118 section 3: Sec-WebSocket-Protocol: sip
    config.disable_auto_reconnect = false;
    config.reconnect_timeout_ms = RECONNECT_TIMEOUT_MS;
    config.network_timeout_ms = NETWORK_TIMEOUT_MS;
    config.ping_interval_sec = PING_INTERVAL_SEC;
    config.pingpong_timeout_sec = PINGPONG_TIMEOUT_SEC;
    // A server close frame (Asterisk restart, proxy) otherwise ends the client
    // task for good even with auto-reconnect enabled (1.7.0 task loop).
    config.enable_close_reconnect = true;
    // Also the TX buffer: keeps an INVITE with SDP in one frame. Larger frames
    // arrive as several DATA events and are reassembled in WsReassembler.
    config.buffer_size = 4096;
    // TLS handshake + certificate verification needs more stack than plain TCP.
    config.task_stack = this->cfg_.tls ? 8192 : 4096;
    if (this->cfg_.tls) {
      if (!this->cfg_.ca_certificate.empty()) {
        // The TLS layer keeps this pointer (no copy), so cfg_ must outlive the client.
        config.cert_pem = this->cfg_.ca_certificate.c_str();
        config.cert_len = 0;  // 0 = NUL-terminated PEM (strlen); non-zero would mean DER
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
  } else {
    // The client exists (task ended, or close() was called earlier): restart it
    // from the helper task so this call never waits for the old task.
    if (!this->restarting_.load(std::memory_order_acquire) && !this->force_restart_()) return false;
  }

  {
    std::lock_guard<std::mutex> lock(this->mutex_);
    this->rx_queue_.clear();
    this->reassembler_.reset();
  }
  // Provisional until connected; loop() adopts the real one.
  this->sent_by_ = sip_random_invalid_host([]() { return random_uint32(); });
  this->sent_by_gen_ = this->generation_.load(std::memory_order_acquire);
  this->down_tracking_ = false;
  this->watchdog_ms_ = WATCHDOG_INITIAL_MS;
  this->started_.store(true, std::memory_order_release);
  if (first_start) {
    esp_err_t err = esp_websocket_client_start(this->client_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_websocket_client_start failed: %d", static_cast<int>(err));
      this->started_.store(false, std::memory_order_release);
      return false;
    }
  }
  ESP_LOGI(TAG, "Connecting to %s", this->uri_.c_str());
  return true;
}

void WsTransport::close() {
  if (this->client_ == nullptr || !this->started_.exchange(false, std::memory_order_acq_rel)) return;
  bool was_connected = this->connected_.exchange(false, std::memory_order_acq_rel);
  // A graceful close can block (close frame send, then waiting for the client
  // task), so only attempt it on a live link; otherwise leave the task alone
  // (the destructor, or a later open(), deals with it off the hot path).
  if (was_connected && !this->restarting_.load(std::memory_order_acquire))
    esp_websocket_client_close(this->client_, pdMS_TO_TICKS(CLOSE_TIMEOUT_MS));
}

bool WsTransport::force_restart_() {
  bool expected = false;
  if (!this->restarting_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return true;
  this->restart_started_ms_ = millis();
  this->restart_warned_ = false;
  this->connected_.store(false, std::memory_order_release);
  BaseType_t ok = xTaskCreate(&WsTransport::restart_task_, "sip_ws_rc", 4096, this, 5, nullptr);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create WebSocket restart task");
    this->restarting_.store(false, std::memory_order_release);
    this->task_gone_.store(true, std::memory_order_release);
    return false;
  }
  return true;
}

// stop() waits (portMAX_DELAY) for the client task, which may sit in a long
// DNS/TLS connect, and for a task that is still winding down after FINISH, so
// the following start() cannot race it. Hence a helper task, never the loop.
void WsTransport::restart_task_(void *arg) {
  auto *self = static_cast<WsTransport *>(arg);
  esp_websocket_client_stop(self->client_);  // ESP_FAIL just means it was already stopped
  esp_err_t err = esp_websocket_client_start(self->client_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_websocket_client_start failed after stop: %d", static_cast<int>(err));
    self->task_gone_.store(true, std::memory_order_release);
  } else {
    self->task_gone_.store(false, std::memory_order_release);
  }
  self->restarting_.store(false, std::memory_order_release);
  vTaskDelete(nullptr);
}

void WsTransport::network_down() {
  // SipClient::loop() skips loop() while WiFi is down; without this the
  // outage would count as link downtime and trigger a restart on recovery.
  this->down_tracking_ = false;
  this->watchdog_ms_ = WATCHDOG_INITIAL_MS;
}

void WsTransport::loop() {
  if (this->client_ == nullptr || !this->started_.load(std::memory_order_acquire)) return;
  // Adopt the sent-by of a new connection (see sent_by_ in the header).
  uint32_t gen = this->generation_.load(std::memory_order_acquire);
  if (gen != this->sent_by_gen_) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    this->sent_by_ = this->pending_sent_by_;
    this->sent_by_gen_ = gen;
  }

  uint32_t now = millis();
  if (this->restarting_.load(std::memory_order_acquire)) {
    if (!this->restart_warned_ && now - this->restart_started_ms_ > RESTART_STUCK_WARN_MS) {
      ESP_LOGW(TAG, "WebSocket restart still in progress (stop() waiting on DNS/TLS?)");
      this->restart_warned_ = true;
    }
    return;
  }
  if (this->connected_.load(std::memory_order_acquire)) {
    this->down_tracking_ = false;
    this->watchdog_ms_ = WATCHDOG_INITIAL_MS;
    return;
  }
  if (this->task_gone_.load(std::memory_order_acquire)) return;  // SipClient will call open()
  if (!this->down_tracking_) {
    this->down_tracking_ = true;
    this->down_since_ms_ = now;
    return;
  }
  if (now - this->down_since_ms_ > this->watchdog_ms_) {
    ESP_LOGW(TAG, "WebSocket down for %u s, restarting the client",
             static_cast<unsigned>(this->watchdog_ms_ / 1000));
    this->watchdog_ms_ = std::min(this->watchdog_ms_ * 2, WATCHDOG_MAX_MS);
    this->down_since_ms_ = now;
    this->force_restart_();
  }
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
        // New connection, new sent-by (picked up by loop(), stable until the next one).
        self->pending_sent_by_ = sip_random_invalid_host([]() { return random_uint32(); });
      }
      self->task_gone_.store(false, std::memory_order_release);
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
    case WEBSOCKET_EVENT_FINISH:
      // The client task is exiting (server close without reconnect, stop(), ...).
      // Never restart from here: this runs on that task. is_open() turns false
      // and a later SipClient::loop() calls open(), which restarts via the helper.
      self->connected_.store(false, std::memory_order_release);
      self->task_gone_.store(true, std::memory_order_release);
      break;
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
