#include "ws_bridge_switch.h"
#include "../ws_bridge.h"
#include "../ws_bridge_entity_json.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.switch";

void WsBridgeSwitch::setup() {
  this->add_on_state_callback([this](bool state) { this->parent_->send_state_bool(this->unique_id_, state); });
}

void WsBridgeSwitch::dump_config() { LOG_SWITCH("", "WS Bridge Switch", this); }

void WsBridgeSwitch::write_state(bool state) { this->publish_state(state); }

void WsBridgeSwitch::ws_bridge_handle_command(const WsCommand &command) {
  if (command.action == "turn_on") {
    this->turn_on();
  } else if (command.action == "turn_off") {
    this->turn_off();
  }
}

void WsBridgeSwitch::ws_bridge_declare() {
  this->parent_->send_entity_declare(this->unique_id_, "switch", this->get_name().str(), this->device_id_,
                                     this->device_name_,
                                     [this](JsonObject root) { add_common_entity_fields(root, *this); });
  if (this->has_state()) this->parent_->send_state_bool(this->unique_id_, this->state);
}

}  // namespace ws_bridge
}  // namespace esphome
