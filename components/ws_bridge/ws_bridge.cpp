#include "ws_bridge.h"
#include <algorithm>
#include "esp_crt_bundle.h"
#include "esp_transport_ws.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge";

void WsBridgeComponent::setup() {
  esp_websocket_client_config_t config = {};
  config.host = this->host_.c_str();
  config.port = this->port_;
  config.path = "/api/websocket";
  config.transport = this->ssl_ ? WEBSOCKET_TRANSPORT_OVER_SSL : WEBSOCKET_TRANSPORT_OVER_TCP;
  if (this->ssl_) {
    config.crt_bundle_attach = esp_crt_bundle_attach;
  }
  config.disable_auto_reconnect = false;
  config.reconnect_timeout_ms = 10000;
  config.network_timeout_ms = 10000;

  this->client_ = esp_websocket_client_init(&config);
  if (this->client_ == nullptr) {
    ESP_LOGE(TAG, "Failed to init WebSocket client");
    this->mark_failed();
    return;
  }
  esp_websocket_register_events(this->client_, WEBSOCKET_EVENT_ANY, WsBridgeComponent::ws_event_handler_, this);
}

void WsBridgeComponent::loop() {
  if (!this->started_) {
    if (!network::is_connected()) return;
    esp_err_t err = esp_websocket_client_start(this->client_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_websocket_client_start failed: %d", err);
      return;
    }
    this->started_ = true;
    this->last_reconnect_attempt_ms_ = millis();
  }

  WsEvent *event;
  while ((event = this->event_queue_.pop()) != nullptr) {
    this->handle_event_(*event);
    this->event_pool_.release(event);
  }

  this->check_liveness_();
}

// Forces a fresh connection attempt, bypassing whatever esp_websocket_client
// thinks it's doing internally. Used both when we've actively determined the
// current connection is dead (ping/pong) and when we've simply been
// disconnected too long (see check_liveness_) — the latter matters because
// the client's own auto-reconnect (disable_auto_reconnect = false) has been
// observed to stop making progress after a prolonged outage (e.g. Home
// Assistant itself restarting, which can take well over a minute), with no
// further event ever firing for us to react to. Without this backstop that
// required power-cycling the ESP to recover.
void WsBridgeComponent::force_reconnect_() {
  this->last_reconnect_attempt_ms_ = millis();
  this->set_state_(WS_BRIDGE_DISCONNECTED);
  esp_websocket_client_stop(this->client_);
  esp_websocket_client_start(this->client_);
}

// Actively probes the connection with HA's standard "ping"/"pong" websocket_api
// commands. Needed because a dead peer (e.g. HA killed without a clean WS
// close — no FIN/RST ever reaches the socket) can otherwise leave the
// underlying esp_websocket_client believing it's still connected indefinitely,
// so is_connected() alone never reports the failure and auto-reconnect never
// kicks in.
void WsBridgeComponent::check_liveness_() {
  uint32_t now = millis();
  if (!this->is_connected()) {
    if (now - this->last_reconnect_attempt_ms_ > this->reconnect_backoff_ms_) {
      ESP_LOGW(TAG, "Still disconnected after %u ms — forcing a fresh connection attempt",
               static_cast<unsigned>(this->reconnect_backoff_ms_));
      this->force_reconnect_();
      this->reconnect_backoff_ms_ = std::min(this->reconnect_backoff_ms_ * 2, this->reconnect_retry_ms_);
    }
    return;
  }
  if (this->ping_outstanding_) {
    if (now - this->last_ping_sent_ms_ > this->pong_timeout_ms_) {
      ESP_LOGW(TAG, "No pong received within %u ms — forcing reconnect",
               static_cast<unsigned>(this->pong_timeout_ms_));
      this->ping_outstanding_ = false;
      this->reconnect_backoff_ms_ = RECONNECT_BACKOFF_BASE_MS;
      this->force_reconnect_();
    }
    return;
  }
  // A periodic re-announce's ws_bridge/connect went unanswered — if HA (not
  // just its ws_bridge integration) is unresponsive, re-sending again on the
  // next interval would just repeat the same no-op forever, so treat this
  // like any other dead-connection signal.
  if (this->awaiting_connect_result_) {
    if (now - this->connect_sent_ms_ > this->pong_timeout_ms_) {
      ESP_LOGW(TAG, "No result for ws_bridge/connect within %u ms — forcing reconnect",
               static_cast<unsigned>(this->pong_timeout_ms_));
      this->awaiting_connect_result_ = false;
      this->reconnect_backoff_ms_ = RECONNECT_BACKOFF_BASE_MS;
      this->force_reconnect_();
    }
    return;
  }
  // See the comment on reannounce_interval_ms_: this doesn't wait for any
  // detected failure, it just periodically re-establishes our registration
  // in case the HA-side integration silently lost track of us while the
  // transport itself (and ping/pong) stayed healthy.
  if (now - this->last_reannounce_ms_ > this->reannounce_interval_ms_) {
    ESP_LOGD(TAG, "Periodic re-announce: resending connect + entity declarations");
    uint32_t connect_id = this->next_id_();
    this->send_raw_(build_connect(connect_id, this->gateway_id_, this->gateway_name_,
                                  this->keep_last_state_on_disconnect_));
    this->last_connect_msg_id_ = connect_id;
    this->awaiting_connect_result_ = true;
    this->connect_sent_ms_ = now;
    this->declare_all_entities_();
    this->last_reannounce_ms_ = now;
  }
  if (now - this->last_ping_sent_ms_ > this->ping_interval_ms_) {
    this->send_raw_(build_ping(this->next_id_()));
    this->ping_outstanding_ = true;
    this->last_ping_sent_ms_ = now;
  }
}

void WsBridgeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge:");
  ESP_LOGCONFIG(TAG, "  Server: %s://%s:%u/api/websocket", this->ssl_ ? "wss" : "ws", this->host_.c_str(),
                this->port_);
  ESP_LOGCONFIG(TAG, "  Gateway ID: %s", this->gateway_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Keep last state on disconnect: %s", YESNO(this->keep_last_state_on_disconnect_));
  ESP_LOGCONFIG(TAG, "  Ping interval: %u ms", static_cast<unsigned>(this->ping_interval_ms_));
  ESP_LOGCONFIG(TAG, "  Pong timeout: %u ms", static_cast<unsigned>(this->pong_timeout_ms_));
  ESP_LOGCONFIG(TAG, "  Reconnect backoff cap: %u ms", static_cast<unsigned>(this->reconnect_retry_ms_));
  ESP_LOGCONFIG(TAG, "  Re-announce interval: %u ms", static_cast<unsigned>(this->reannounce_interval_ms_));
}

// May be called from either the main loop task or the esp_websocket_client
// task (see ws_event_handler_), so this must be a single atomic RMW rather
// than a load-compare-store.
void WsBridgeComponent::set_state_(WsBridgeState s) {
  WsBridgeState old = this->state_.exchange(s, std::memory_order_acq_rel);
  if (old != s) {
    ESP_LOGD(TAG, "state %d -> %d", old, s);
  }
}

// Runs on the esp_websocket_client task, NOT the ESPHome main loop task.
// Must stay fast and non-blocking: reassemble fragmented text frames into
// rx_accum_ (producer-only state) and hand off complete events through the
// lock-free queue for loop() to process.
void WsBridgeComponent::ws_event_handler_(void *handler_args, esp_event_base_t base, int32_t event_id,
                                          void *event_data) {
  auto *self = static_cast<WsBridgeComponent *>(handler_args);
  auto *data = static_cast<esp_websocket_event_data_t *>(event_data);
  auto ws_event_id = static_cast<esp_websocket_event_id_t>(event_id);

  bool was_connected = false;

  // A (re)connect or drop always starts a fresh message stream: discard any
  // partial fragment left over from a message that never completed (e.g. the
  // socket dropped mid-fragment), so it can't get concatenated with data from
  // a later connection.
  //
  // The state_ transition itself also happens right here, unconditionally,
  // rather than being deferred to loop()'s consumption of the queued event.
  // event_queue_ is bounded (EVENT_QUEUE_SIZE) and silently drops events when
  // full (see below) — if a dropped event were the only place a disconnect
  // got recorded, state_ could stay stuck at WS_BRIDGE_CONNECTED across a
  // reconnect. is_connected() would then still report true on the fresh,
  // not-yet-authenticated socket, so a state push could go out before HA even
  // sends auth_required, which HA's auth handler correctly rejects. Doing the
  // transition here means it can never be lost to a full queue; only the
  // (non-critical) log line / callback in handle_event_ can be.
  if (ws_event_id == WEBSOCKET_EVENT_CONNECTED) {
    self->rx_accum_.clear();
    self->set_state_(WS_BRIDGE_WAIT_AUTH_REQUIRED);
  } else if (ws_event_id == WEBSOCKET_EVENT_DISCONNECTED || ws_event_id == WEBSOCKET_EVENT_ERROR ||
             ws_event_id == WEBSOCKET_EVENT_CLOSED) {
    self->rx_accum_.clear();
    was_connected = (self->state_.exchange(WS_BRIDGE_DISCONNECTED, std::memory_order_acq_rel) == WS_BRIDGE_CONNECTED);
  }

  if (ws_event_id == WEBSOCKET_EVENT_DATA) {
    if (data->op_code != WS_TRANSPORT_OPCODES_TEXT && data->op_code != WS_TRANSPORT_OPCODES_CONT) {
      return;  // ignore ping/pong/close/binary frames
    }
    if (data->data_len > 0) self->rx_accum_.append(data->data_ptr, data->data_len);
    bool complete = data->payload_len == 0 || (data->payload_offset + data->data_len >= data->payload_len);
    if (!complete) return;  // wait for more fragments
  }

  WsEvent *event = self->event_pool_.allocate();
  if (event == nullptr) return;  // queue full, drop this event (state_ is already correct regardless)
  event->event_id = ws_event_id;
  event->was_connected = was_connected;
  if (ws_event_id == WEBSOCKET_EVENT_DATA) {
    event->data = std::move(self->rx_accum_);
    self->rx_accum_.clear();
  }
  if (!self->event_queue_.push(event)) self->event_pool_.release(event);
}

void WsBridgeComponent::handle_event_(const WsEvent &event) {
  switch (event.event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      // state_ is already WS_BRIDGE_WAIT_AUTH_REQUIRED — set by
      // ws_event_handler_ itself, see the comment there.
      ESP_LOGI(TAG, "WebSocket transport connected");
      break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_ERROR:
    case WEBSOCKET_EVENT_CLOSED:
      // state_ is already WS_BRIDGE_DISCONNECTED — set by ws_event_handler_
      // itself. event.was_connected is a snapshot taken at that time; by now
      // is_connected() would always read false, so it can't be used here to
      // tell whether this is a real transition.
      if (event.was_connected) {
        ESP_LOGW(TAG, "WebSocket disconnected");
        // Anchor the reconnect backoff to this disconnect, not to whatever
        // earlier attempt last_reconnect_attempt_ms_ still held — otherwise
        // a connection that drops shortly after connecting could sit idle
        // for however much of reconnect_retry_ms_ was already "used up"
        // since that earlier attempt, instead of retrying promptly. See the
        // comment on reconnect_backoff_ms_ in ws_bridge.h.
        this->last_reconnect_attempt_ms_ = millis();
        this->reconnect_backoff_ms_ = RECONNECT_BACKOFF_BASE_MS;
        this->disconnected_cb_.call();
      }
      break;
    case WEBSOCKET_EVENT_DATA:
      this->handle_message_(event.data);
      break;
    default:
      break;
  }
}

void WsBridgeComponent::handle_message_(const std::string &raw) {
  ParsedMessage msg = parse_message(raw);

  if (msg.type == "auth_required") {
    this->send_raw_(build_auth(this->token_));
    this->set_state_(WS_BRIDGE_WAIT_AUTH_OK);
  } else if (msg.type == "auth_ok") {
    this->send_raw_(
        build_connect(this->next_id_(), this->gateway_id_, this->gateway_name_, this->keep_last_state_on_disconnect_));
    this->set_state_(WS_BRIDGE_CONNECTED);
    this->ping_outstanding_ = false;
    this->reconnect_backoff_ms_ = RECONNECT_BACKOFF_BASE_MS;
    this->last_ping_sent_ms_ = millis();
    this->last_reannounce_ms_ = this->last_ping_sent_ms_;
    this->declare_all_entities_();
    this->connected_cb_.call();
  } else if (msg.type == "auth_invalid") {
    ESP_LOGE(TAG, "Home Assistant rejected the access token");
  } else if (msg.type == "pong") {
    this->ping_outstanding_ = false;
  } else if (msg.type == "event") {
    if (!msg.command.unique_id.empty()) this->route_command_(msg.command);
  } else if (msg.type == "result") {
    // Only the periodic re-announce's ws_bridge/connect is tracked (see
    // check_liveness_); every other ws_bridge/* result is fire-and-forget.
    if (this->awaiting_connect_result_ && msg.id == this->last_connect_msg_id_) {
      this->awaiting_connect_result_ = false;
    }
  }
}

void WsBridgeComponent::route_command_(const WsCommand &command) {
  for (auto *device : this->devices_) {
    if (device->get_ws_bridge_unique_id() == command.unique_id) {
      device->ws_bridge_handle_command(command);
      return;
    }
  }
  ESP_LOGW(TAG, "Command for unknown unique_id '%s'", command.unique_id.c_str());
}

void WsBridgeComponent::declare_all_entities_() {
  for (auto *device : this->devices_) device->ws_bridge_declare();
}

void WsBridgeComponent::send_raw_(const std::string &msg) {
  if (this->client_ == nullptr || !esp_websocket_client_is_connected(this->client_)) return;
  esp_websocket_client_send_text(this->client_, msg.c_str(), msg.size(), pdMS_TO_TICKS(1000));
}

void WsBridgeComponent::send_entity_declare(const std::string &unique_id, const std::string &platform,
                                            const std::string &name, const std::string &device_id,
                                            const std::string &device_name,
                                            const std::function<void(JsonObject)> &extra) {
  if (!this->is_connected()) return;
  this->send_raw_(build_entity_declare(this->next_id_(), unique_id, platform, name, device_id, device_name, extra));
}

void WsBridgeComponent::send_state_float(const std::string &unique_id, float value) {
  if (!this->is_connected()) return;
  this->send_raw_(build_state_float(this->next_id_(), unique_id, value));
}

void WsBridgeComponent::send_state_bool(const std::string &unique_id, bool value) {
  if (!this->is_connected()) return;
  this->send_raw_(build_state_bool(this->next_id_(), unique_id, value));
}

void WsBridgeComponent::send_state_string(const std::string &unique_id, const std::string &value) {
  if (!this->is_connected()) return;
  this->send_raw_(build_state_string(this->next_id_(), unique_id, value));
}

}  // namespace ws_bridge
}  // namespace esphome
