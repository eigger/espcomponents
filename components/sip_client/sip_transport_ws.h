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
// Reconnect: the client's built-in auto-reconnect handles ordinary drops
// (enable_close_reconnect also covers a server-sent close frame). Two
// backstops restart the client from a short-lived helper task, because
// esp_websocket_client_stop() waits forever for the client task and must never
// run on the main loop: (1) the client task exited (WEBSOCKET_EVENT_FINISH),
// (2) the link has been down for minutes without recovering. The helper's
// stop() also waits out a task that is still finishing, so start() cannot race
// it. SipClient notices drops via connected()/generation() and re-registers.
class WsTransport : public SipTransport {
 public:
  explicit WsTransport(const WsTransportConfig &cfg) : cfg_(cfg) {}
  ~WsTransport() override;

  bool open(const std::string &server, uint16_t port) override;
  void close() override;
  void loop() override;
  void network_down() override;
  // False once the client task is gone, so SipClient calls open() again (which
  // restarts it on a later loop iteration).
  bool is_open() const override {
    return this->started_.load(std::memory_order_acquire) &&
           (!this->task_gone_.load(std::memory_order_acquire) || this->restarting_.load(std::memory_order_acquire));
  }
  bool connected() const override {
    return this->started_.load(std::memory_order_acquire) && this->connected_.load(std::memory_order_acquire);
  }
  // The generation loop() has adopted (main loop only), not the raw counter:
  // SipClient reads it right after loop(), and the sent-by in the first REGISTER
  // of a new connection must already be the new one.
  uint32_t generation() const override { return this->sent_by_gen_; }
  bool send(const std::string &msg) override;
  bool poll(std::string &out) override;
  const char *via_token() const override { return this->cfg_.tls ? "WSS" : "WS"; }
  std::string via_sent_by() const override { return this->sent_by_; }
  std::string contact_host_port() const override { return this->via_sent_by(); }
  const char *contact_transport_param() const override {
    return this->cfg_.tls ? ";transport=wss" : ";transport=ws";
  }
  std::string local_ip() const override;

 protected:
  static void event_handler_(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
  void on_data_(const esp_websocket_event_data_t *data);
  // False when the helper task could not be created (the caller backs off).
  bool force_restart_();
  static void restart_task_(void *arg);

  static constexpr size_t MAX_MESSAGE = 8192;
  static constexpr size_t MAX_QUEUE = 8;

  WsTransportConfig cfg_;  // cfg_.ca_certificate is referenced (not copied) by the TLS layer
  std::string uri_;
  esp_websocket_client_handle_t client_{nullptr};
  std::atomic<bool> started_{false};
  std::atomic<bool> connected_{false};
  std::atomic<uint32_t> generation_{0};
  std::atomic<bool> task_gone_{false};   // client task exited (FINISH) and was not restarted yet
  std::atomic<bool> restarting_{false};  // restart_task_ is running

  // Main-loop only: liveness watchdog.
  uint32_t down_since_ms_{0};
  bool down_tracking_{false};
  uint32_t watchdog_ms_{0};
  uint32_t restart_started_ms_{0};
  bool restart_warned_{false};
  // Main-loop only: the sent-by in use. It only changes in loop(), so a Via and
  // a Contact built within one SipClient::loop() always agree.
  std::string sent_by_;
  uint32_t sent_by_gen_{0};

  mutable std::mutex mutex_;  // guards the members below
  std::deque<std::string> rx_queue_;
  WsReassembler reassembler_{MAX_MESSAGE};
  std::string pending_sent_by_;  // written by the client task on CONNECTED
};

}  // namespace sip_client
}  // namespace esphome

#endif  // USE_SIP_CLIENT_WS
