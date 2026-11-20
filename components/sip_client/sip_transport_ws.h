#pragma once
#include "esphome/core/defines.h"

#ifdef USE_SIP_CLIENT_WS

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include "esp_websocket_client.h"
#include "sip_transport.h"
#include "sip_ws_util.h"

namespace esphome {
namespace sip_client {

struct WsTransportConfig {
  bool tls{true};
  std::string path{"/ws"};
  bool verify_ssl{true};
  std::string ca_certificate;  // PEM; overrides the built-in certificate bundle
};

// SIP over WebSocket (RFC 7118) on top of ESP-IDF's esp_websocket_client.
//
// Threading: the client runs its own task and invokes event_handler_() there
// (occasionally inline on the caller of stop/send). That handler only touches
// atomics and mutex_-guarded state (RX queue, reassembler, sent-by). Everything
// else is main-loop only, per SipTransport's contract.
//
// Reconnect: the client's built-in auto-reconnect is used (reconnect_timeout_ms)
// instead of a manual stop()/start(): stop() blocks until the client task
// leaves a possibly long DNS/TLS connect, which must not happen on the main
// loop. SipClient notices drops via connected()/generation() and re-registers.
class WsTransport : public SipTransport {
 public:
  explicit WsTransport(const WsTransportConfig &cfg) : cfg_(cfg) {}
  ~WsTransport() override;

  bool open(const std::string &server, uint16_t port) override;
  void close() override;
  bool is_open() const override { return this->started_.load(std::memory_order_acquire); }
  bool connected() const override {
    return this->started_.load(std::memory_order_acquire) && this->connected_.load(std::memory_order_acquire);
  }
  uint32_t generation() const override { return this->generation_.load(std::memory_order_acquire); }
  bool send(const std::string &msg) override;
  bool poll(std::string &out) override;
  const char *via_token() const override { return this->cfg_.tls ? "WSS" : "WS"; }
  std::string via_sent_by() const override;
  std::string contact_host_port() const override { return this->via_sent_by(); }
  const char *contact_transport_param() const override {
    return this->cfg_.tls ? ";transport=wss" : ";transport=ws";
  }
  std::string local_ip() const override;

 protected:
  static void event_handler_(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
  void on_data_(const esp_websocket_event_data_t *data);

  static constexpr size_t MAX_MESSAGE = 8192;
  static constexpr size_t MAX_QUEUE = 8;

  WsTransportConfig cfg_;  // cfg_.ca_certificate is referenced (not copied) by the TLS layer
  std::string uri_;
  esp_websocket_client_handle_t client_{nullptr};
  std::atomic<bool> started_{false};
  std::atomic<bool> connected_{false};
  std::atomic<uint32_t> generation_{0};

  mutable std::mutex mutex_;  // guards the three members below
  std::deque<std::string> rx_queue_;
  WsReassembler reassembler_{MAX_MESSAGE};
  std::string sent_by_;
};

}  // namespace sip_client
}  // namespace esphome

#endif  // USE_SIP_CLIENT_WS
