#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/event_pool.h"
#include "esphome/core/lock_free_queue.h"
#include "esp_websocket_client.h"
#include "ws_bridge_device.h"
#include "ws_protocol.h"

namespace esphome {
namespace ws_bridge {

enum WsBridgeState : uint8_t {
  WS_BRIDGE_DISCONNECTED = 0,
  WS_BRIDGE_WAIT_AUTH_REQUIRED,
  WS_BRIDGE_WAIT_AUTH_OK,
  WS_BRIDGE_CONNECTED,
};

// One transport-level event handed from the esp_websocket_client task to the
// main loop via a lock-free SPSC queue. Text payloads (WEBSOCKET_EVENT_DATA)
// are fully reassembled (across WS fragments) before being queued, so `data`
// is always either empty or one complete JSON message.
struct WsEvent {
  esp_websocket_event_id_t event_id{WEBSOCKET_EVENT_ERROR};
  std::string data;
  // Snapshot (taken by the producer at the moment of a disconnect-family
  // event) of whether the connection was actually up before this event.
  // The consumer uses this instead of re-reading state_, because state_ has
  // already been updated by the producer by the time the consumer gets to
  // it — and the queued event itself may never be dequeued at all if the
  // queue was full, so it must not be the thing state_ correctness depends
  // on.
  bool was_connected{false};
  void release() {
    data.clear();
    was_connected = false;
  }
};

class WsBridgeComponent : public Component {
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_ssl(bool ssl) { this->ssl_ = ssl; }
  void set_token(const std::string &token) { this->token_ = token; }
  void set_gateway_id(const std::string &id) { this->gateway_id_ = id; }
  void set_gateway_name(const std::string &name) { this->gateway_name_ = name; }
  void set_keep_last_state_on_disconnect(bool v) { this->keep_last_state_on_disconnect_ = v; }

  void add_on_connected_callback(std::function<void()> &&cb) { this->connected_cb_.add(std::move(cb)); }
  void add_on_disconnected_callback(std::function<void()> &&cb) { this->disconnected_cb_.add(std::move(cb)); }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void register_device(WsBridgeDevice *device) { this->devices_.push_back(device); }
  // state_ is written from two tasks (the esp_websocket_client task on
  // disconnect/connect, the main loop task on auth progress), so it must be
  // atomic. See ws_event_handler_ for why the disconnect-family transitions
  // in particular can't wait for the main loop to process a queued event.
  bool is_connected() const { return this->state_.load(std::memory_order_acquire) == WS_BRIDGE_CONNECTED; }

  // Called by platform entities (via WsBridgeDevice helpers) to push state
  // and declarations. No-ops while not connected; the next (re)connect will
  // re-declare and re-push through ws_bridge_declare().
  void send_state_float(const std::string &unique_id, float value);
  void send_state_bool(const std::string &unique_id, bool value);
  void send_state_string(const std::string &unique_id, const std::string &value);
  void send_entity_declare(const std::string &unique_id, const std::string &platform, const std::string &name,
                           const std::string &device_id, const std::string &device_name,
                           const std::function<void(JsonObject)> &extra);

 protected:
  static void ws_event_handler_(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
  void handle_event_(const WsEvent &event);
  void handle_message_(const std::string &raw);
  void route_command_(const WsCommand &command);
  void declare_all_entities_();
  void send_raw_(const std::string &msg);
  uint32_t next_id_() { return ++this->msg_id_; }
  void set_state_(WsBridgeState s);
  void check_liveness_();
  void force_reconnect_();

  esp_websocket_client_handle_t client_{nullptr};
  bool started_{false};

  std::string host_;
  uint16_t port_{8123};
  bool ssl_{true};
  std::string token_;
  std::string gateway_id_;
  std::string gateway_name_;
  bool keep_last_state_on_disconnect_{false};

  std::atomic<WsBridgeState> state_{WS_BRIDGE_DISCONNECTED};
  uint32_t msg_id_{0};
  std::vector<WsBridgeDevice *> devices_{};

  // App-level ping/pong keepalive (see check_liveness_()). Detects a dead
  // connection that the transport layer doesn't notice on its own. Interval
  // is kept low-frequency (one tiny JSON round-trip a minute) since this is
  // just a dead-connection backstop, not a latency-sensitive heartbeat; the
  // timeout leaves generous headroom for WAN paths (Nabu Casa remote UI,
  // reverse proxy) where a multi-second round trip is normal, not a fault.
  uint32_t last_ping_sent_ms_{0};
  bool ping_outstanding_{false};
  static constexpr uint32_t PING_INTERVAL_MS = 60000;
  static constexpr uint32_t PONG_TIMEOUT_MS = 15000;

  // Backstop for check_liveness_(): while disconnected, esp_websocket_client's
  // own auto-reconnect can stop making progress after a prolonged outage
  // (e.g. HA itself restarting) without ever raising another event we could
  // react to. If we've been down longer than this, force a fresh connection
  // attempt ourselves rather than waiting on the library indefinitely.
  uint32_t last_reconnect_attempt_ms_{0};
  static constexpr uint32_t RECONNECT_RETRY_MS = 120000;

  // Producer-side (WS client task) fragment reassembly buffer. Only ever
  // touched from ws_event_handler_(), never from loop() — no locking needed.
  std::string rx_accum_;

  static constexpr uint8_t EVENT_QUEUE_SIZE = 8;
  EventPool<WsEvent, EVENT_QUEUE_SIZE - 1> event_pool_;
  LockFreeQueue<WsEvent, EVENT_QUEUE_SIZE> event_queue_;

  CallbackManager<void()> connected_cb_{};
  CallbackManager<void()> disconnected_cb_{};
};

}  // namespace ws_bridge
}  // namespace esphome
