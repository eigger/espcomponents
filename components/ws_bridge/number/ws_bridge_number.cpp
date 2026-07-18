#include "ws_bridge_number.h"
#include "../ws_bridge.h"
#include "../ws_bridge_entity_json.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.number";

void WsBridgeNumber::setup() {
  this->add_on_state_callback([this](float state) { this->parent_->send_state_float(this->unique_id_, state); });
}

void WsBridgeNumber::dump_config() { LOG_NUMBER("", "WS Bridge Number", this); }

void WsBridgeNumber::control(float value) { this->publish_state(value); }

void WsBridgeNumber::ws_bridge_handle_command(const WsCommand &command) {
  if (command.action == "set_value" && command.has_value) {
    this->make_call().set_value(command.value_float).perform();
  }
}

void WsBridgeNumber::ws_bridge_declare() {
  this->parent_->send_entity_declare(
      this->unique_id_, "number", this->get_name().str(), this->device_id_, this->device_name_,
      [this](JsonObject root) {
        add_common_entity_fields(root, *this);
        root["min"] = this->traits.get_min_value();
        root["max"] = this->traits.get_max_value();
        root["step"] = this->traits.get_step();
      });
  if (this->has_state()) this->parent_->send_state_float(this->unique_id_, this->state);
}

}  // namespace ws_bridge
}  // namespace esphome
