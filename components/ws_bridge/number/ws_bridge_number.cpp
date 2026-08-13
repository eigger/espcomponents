#include "ws_bridge_number.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.number";

void WsBridgeNumber::setup() { ws_subscribe_number(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeNumber::dump_config() {
  LOG_NUMBER("", "WS Bridge Number", this);
  if (this->source_ != nullptr) ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

// Only reached when NOT wrapping — see WsBridgeSwitch::write_state().
void WsBridgeNumber::control(float value) { this->publish_state(value); }

void WsBridgeNumber::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_number(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeNumber::ws_bridge_declare() {
  number::Number &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_number(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_number(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
