#include "ws_bridge_fan.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.fan";

void WsBridgeFan::setup() { ws_subscribe_fan(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeFan::dump_config() {
  LOG_FAN("", "WS Bridge Fan", this);
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

fan::FanTraits WsBridgeFan::get_traits() {
  if (this->source_ != nullptr)
    return this->source_->get_traits();
  return fan::FanTraits(false, true, false, 100);
}

// Only reached when NOT wrapping — see WsBridgeSwitch::write_state().
void WsBridgeFan::control(const fan::FanCall &call) {
  if (call.get_state().has_value())
    this->state = *call.get_state();
  if (call.get_speed().has_value())
    this->speed = *call.get_speed();
  if (call.get_oscillating().has_value())
    this->oscillating = *call.get_oscillating();
  if (call.get_direction().has_value())
    this->direction = *call.get_direction();
  if (!call.get_preset_mode().empty())
    this->preset_mode = call.get_preset_mode();
  this->publish_state();
}

void WsBridgeFan::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_fan(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeFan::ws_bridge_declare() {
  fan::Fan &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_fan(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_fan(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
