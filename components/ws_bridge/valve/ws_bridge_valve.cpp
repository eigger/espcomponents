#include "ws_bridge_valve.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.valve";

void WsBridgeValve::setup() { ws_subscribe_valve(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeValve::dump_config() {
  LOG_VALVE("", "WS Bridge Valve", this);
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

valve::ValveTraits WsBridgeValve::get_traits() {
  if (this->source_ != nullptr)
    return this->source_->get_traits();
  valve::ValveTraits traits;
  traits.set_supports_position(true);
  traits.set_supports_stop(true);
  return traits;
}

// Only reached when NOT wrapping — see WsBridgeSwitch::write_state().
void WsBridgeValve::control(const valve::ValveCall &call) {
  if (call.get_stop()) {
    this->current_operation = valve::VALVE_OPERATION_IDLE;
  } else if (call.get_position().has_value()) {
    this->position = *call.get_position();
    this->current_operation = valve::VALVE_OPERATION_IDLE;
  }
  this->publish_state();
}

void WsBridgeValve::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_valve(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeValve::ws_bridge_declare() {
  valve::Valve &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_valve(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_valve(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
