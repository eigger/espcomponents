#pragma once
#include <atomic>
#include <deque>
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
  // Empty = default: ESPHome version + compilation time. A non-empty value is
  // sent as-is on ws_bridge/connect (HA gateway device sw_version).
  void set_sw_version(const std::string &v) { this->sw_version_ = v; }
  void set_manufacturer(const std::string &v) { this->manufacturer_ = v; }
  void set_model(const std::string &v) { this->model_ = v; }
  void set_hw_version(const std::string &v) { this->hw_version_ = v; }
  void set_keep_last_state_on_disconnect(bool v) { this->keep_last_state_on_disconnect_ = v; }
  void set_sync_entities(bool v) { this->sync_entities_ = v; }
  // See check_liveness_() for what these govern.
  void set_ping_interval(uint32_t ms) { this->ping_interval_ms_ = ms; }
  void set_pong_timeout(uint32_t ms) { this->pong_timeout_ms_ = ms; }
  void set_reconnect_timeout(uint32_t ms) { this->reconnect_retry_ms_ = ms; }
  void set_reannounce_interval(uint32_t ms) { this->reannounce_interval_ms_ = ms; }

  void add_on_connected_callback(std::function<void()> &&cb) { this->connected_cb_.add(std::move(cb)); }
  void add_on_disconnected_callback(std::function<void()> &&cb) { this->disconnected_cb_.add(std::move(cb)); }
  // Fires wherever the registered platform entities re-declare themselves —
  // i.e. on connect AND on every periodic re-announce. Manual (lambda-built)
  // declarations must hook this rather than on_connected, so that a
  // re-announce which heals a lost HA-side registration re-declares them too.
  void add_on_declare_callback(std::function<void()> &&cb) { this->declare_cb_.add(std::move(cb)); }

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
  //
  // These are also the escape hatch for declaring entity types ESPHome itself
  // has no domain for (e.g. device_tracker): call them straight from a YAML
  // lambda. Manually declared entities are NOT re-declared automatically on
  // reconnect — drive them from the hub's on_connected: trigger. See README.
  void send_state_float(const std::string &unique_id, float value);
  void send_state_bool(const std::string &unique_id, bool value);
  void send_state_string(const std::string &unique_id, const std::string &value);
  void send_state_object(const std::string &unique_id, const std::function<void(JsonObject)> &value_fn);
  void send_entity_declare(const std::string &unique_id, const std::string &platform, const std::string &name,
                           const std::string &device_id, const std::string &device_name,
                           const std::function<void(JsonObject)> &extra);

 protected:
  static void ws_event_handler_(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
  void handle_event_(const WsEvent &event);
  void handle_message_(const std::string &raw);
  void route_command_(const WsCommand &command);
  void start_declare_pass_(bool run_connected_and_sync);
  void progress_declare_();
  void maybe_flush_sync_();
  void flush_sync_();
  // Non-blocking send with a drainable queue. Returns false only when the
  // message could not be accepted (not connected, or queue full).
  bool send_raw_(const std::string &msg, const std::string &sync_declare_uid = "");
  void drain_tx_queue_();
  void clear_tx_queue_();
  uint32_t next_id_() { return ++this->msg_id_; }
  void set_state_(WsBridgeState s);
  void check_liveness_();
  void force_reconnect_();
  static void reconnect_task_(void *arg);
  std::string effective_sw_version_();
  void send_connect_(uint32_t id);
  uint32_t reconnect_backoff_base_() const;

  // One outbound WS text frame. `sync_declare_uid` is set for entity declares
  // collected during a sync pass — only appended to declared_ids_ after the
  // frame is actually written to the socket (not when merely queued).
  struct TxItem {
    std::string msg;
    std::string sync_declare_uid;
  };

  esp_websocket_client_handle_t client_{nullptr};
  bool started_{false};

  std::string host_;
  uint16_t port_{8123};
  bool ssl_{true};
  std::string token_;
  std::string gateway_id_;
  std::string gateway_name_;
  std::string sw_version_;
  std::string manufacturer_;
  std::string model_;
  std::string hw_version_;
  bool keep_last_state_on_disconnect_{false};

  // Opt-in (sync_entities:). After declaring everything on connect, tell HA the
  // full set of unique_ids we provide so it can drop entities we no longer
  // declare — see build_sync(). Off by default because it deletes HA-side
  // entities: the list has to genuinely be everything this gateway offers, and
  // only the config author can promise that. Entities declared from anywhere
  // other than the declare/connect pass (say, an interval or a button press)
  // would be missing from it and get removed.
  //
  // Collection runs for the whole declare window and is closed by flush_sync_(),
  // so it captures on_declare: AND on_connected: lambdas, not just the
  // registered platform entities. Sent once per connection, not on every
  // periodic re-announce — a stale entity is not urgent enough to make HA scan
  // its entity registry every reannounce_interval.
  bool sync_entities_{false};
  bool collecting_declared_ids_{false};
  std::vector<std::string> declared_ids_{};

  // Paced (re)declare: platform entities are declared a few per loop() so a
  // large gateway cannot block the main loop for tens of seconds on a slow
  // link. on_declare: / on_connected: run after the device list is finished;
  // ws_bridge/sync waits until the TX queue has drained so declared_ids_ only
  // contains frames that actually left the socket.
  //
  // send_text timeout must NOT be 0 (or tiny): esp_websocket_client 1.7.0
  // treats a 0-byte write (tcp window full past the wait) as fatal and calls
  // abort_connection(). That turns declare bursts into reconnect storms.
  // Keep one/few sends per loop with a generous wait instead.
  bool declare_in_progress_{false};
  size_t declare_device_index_{0};
  bool declare_run_connected_{false};
  bool sync_flush_pending_{false};
  static constexpr size_t DECLARE_DEVICES_PER_LOOP = 1;

  std::deque<TxItem> tx_queue_{};
  static constexpr size_t TX_QUEUE_MAX = 64;
  static constexpr size_t TX_PER_LOOP = 1;
  static constexpr uint32_t TX_SEND_TIMEOUT_MS = 1000;

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
  uint32_t ping_interval_ms_{60000};
  uint32_t pong_timeout_ms_{15000};

  // Backstop for check_liveness_(): while disconnected, esp_websocket_client's
  // own auto-reconnect can stop making progress after a prolonged outage
  // (e.g. HA itself restarting) without ever raising another event we could
  // react to. If we've been down longer than the current backoff, force a
  // fresh connection attempt ourselves rather than waiting on the library
  // indefinitely.
  //
  // last_reconnect_attempt_ms_ is reset at every disconnect (not just at each
  // attempt) — see handle_event_()/force_reconnect_() — precisely so this
  // isn't measured from some earlier, unrelated attempt. Without that, a
  // connection that drops shortly after connecting could sit idle for up to
  // reconnect_retry_ms_ minus however much of that window had already
  // elapsed before the drop, rather than retrying promptly.
  //
  // reconnect_backoff_ms_ is the actual delay used each time: starts at
  // min(RECONNECT_BACKOFF_BASE_MS, reconnect_retry_ms_) so a configured
  // reconnect_timeout below 30s is not first waited as 30s then shrunk, and
  // doubles on every failed attempt if reconnect_retry_ms_ is higher than
  // the base, then resets back to base as soon as we're connected again
  // (or on any freshly-detected disconnect).
  uint32_t last_reconnect_attempt_ms_{0};
  uint32_t reconnect_retry_ms_{30000};
  static constexpr uint32_t RECONNECT_BACKOFF_BASE_MS = 30000;
  uint32_t reconnect_backoff_ms_{RECONNECT_BACKOFF_BASE_MS};
  // stop()/start() wait with portMAX_DELAY and can block for tens of seconds
  // (DNS / TLS). Must not run on the ESPHome loop task (task WDT panic).
  std::atomic<bool> reconnect_task_busy_{false};
  uint32_t reconnect_task_started_ms_{0};
  bool reconnect_stuck_warned_{false};
  static constexpr uint32_t RECONNECT_STUCK_WARN_MS = 30000;

  // Backstop for a different failure mode: the transport (and HA's generic
  // websocket_api ping/pong) can stay perfectly alive while the ws_bridge
  // *integration* on the HA side loses track of this specific gateway (e.g.
  // its config entry got reloaded independently of the raw connection, during
  // a long/messy HA restart). Ping/pong alone can't detect this — HA core
  // answers pings regardless of whether our integration still recognizes the
  // connection — so ws_bridge/state pushes would then be silently dropped
  // forever with no disconnect ever observed. Periodically resending
  // ws_bridge/connect + full entity/state declarations re-registers with
  // whatever WsBridge instance is currently live, healing this without
  // needing to detect it precisely.
  //
  // 60s (not a longer, lower-overhead interval) matches the companion
  // hass-ble-android client's HaWsClient.resubscribeJob, which independently
  // arrived at the same self-heal against the same HA-side failure mode.
  uint32_t last_reannounce_ms_{0};
  uint32_t reannounce_interval_ms_{60000};

  // Set when a ws_bridge/connect sent by the periodic re-announce above is
  // awaiting its "result" reply. If HA never answers it (e.g. because the
  // whole HA side, not just its ws_bridge integration, is unresponsive),
  // blindly re-sending on the next interval would just repeat the same
  // no-op forever — so a timeout here forces a full transport reconnect
  // instead. Mirrors HaWsClient's startBridgeTimeout()/onConnectResult().
  bool awaiting_connect_result_{false};
  uint32_t connect_sent_ms_{0};
  uint32_t last_connect_msg_id_{0};

  // Producer-side (WS client task) fragment reassembly buffer. Only ever
  // touched from ws_event_handler_(), never from loop() — no locking needed.
  std::string rx_accum_;

  static constexpr uint8_t EVENT_QUEUE_SIZE = 8;
  EventPool<WsEvent, EVENT_QUEUE_SIZE - 1> event_pool_;
  LockFreeQueue<WsEvent, EVENT_QUEUE_SIZE> event_queue_;

  CallbackManager<void()> connected_cb_{};
  CallbackManager<void()> disconnected_cb_{};
  CallbackManager<void()> declare_cb_{};
};

}  // namespace ws_bridge
}  // namespace esphome
