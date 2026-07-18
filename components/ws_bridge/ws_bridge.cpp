#include "ws_bridge.h"
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
  }

  WsEvent *event;
  while ((event = this->event_queue_.pop()) != nullptr) {
    this->handle_event_(*event);
    this->event_pool_.release(event);
  }
}

void WsBridgeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge:");
  ESP_LOGCONFIG(TAG, "  Server: %s://%s:%u/api/websocket", this->ssl_ ? "wss" : "ws", this->host_.c_str(),
                this->port_);
  ESP_LOGCONFIG(TAG, "  Gateway ID: %s", this->gateway_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Keep last state on disconnect: %s", YESNO(this->keep_last_state_on_disconnect_));
}

void WsBridgeComponent::set_state_(WsBridgeState s) {
  if (this->state_ != s) {
    ESP_LOGD(TAG, "state %d -> %d", this->state_, s);
    this->state_ = s;
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

  // A (re)connect or drop always starts a fresh message stream: discard any
  // partial fragment left over from a message that never completed (e.g. the
  // socket dropped mid-fragment), so it can't get concatenated with data from
  // a later connection.
  if (ws_event_id == WEBSOCKET_EVENT_CONNECTED || ws_event_id == WEBSOCKET_EVENT_DISCONNECTED ||
      ws_event_id == WEBSOCKET_EVENT_ERROR || ws_event_id == WEBSOCKET_EVENT_CLOSED) {
    self->rx_accum_.clear();
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
  if (event == nullptr) return;  // queue full, drop this event
  event->event_id = ws_event_id;
  if (ws_event_id == WEBSOCKET_EVENT_DATA) {
    event->data = std::move(self->rx_accum_);
    self->rx_accum_.clear();
  }
  if (!self->event_queue_.push(event)) self->event_pool_.release(event);
}

void WsBridgeComponent::handle_event_(const WsEvent &event) {
  switch (event.event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      ESP_LOGI(TAG, "WebSocket transport connected");
      this->set_state_(WS_BRIDGE_WAIT_AUTH_REQUIRED);
      break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_ERROR:
    case WEBSOCKET_EVENT_CLOSED: {
      bool was_connected = this->is_connected();
      this->set_state_(WS_BRIDGE_DISCONNECTED);
      if (was_connected) {
        ESP_LOGW(TAG, "WebSocket disconnected");
        this->disconnected_cb_.call();
      }
      break;
    }
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
    this->declare_all_entities_();
    this->connected_cb_.call();
  } else if (msg.type == "auth_invalid") {
    ESP_LOGE(TAG, "Home Assistant rejected the access token");
  } else if (msg.type == "event") {
    if (!msg.command.unique_id.empty()) this->route_command_(msg.command);
  }
  // "result": nothing to do, we don't block on ws_bridge/* results.
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
