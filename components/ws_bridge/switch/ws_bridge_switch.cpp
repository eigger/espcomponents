#include "ws_bridge_switch.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.switch";

void WsBridgeSwitch::setup() { ws_subscribe_switch(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeSwitch::dump_config() {
  LOG_SWITCH("", "WS Bridge Switch", this);
  if (this->source_ != nullptr) ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

// Only reached when NOT wrapping: a wrapped switch's commands are routed to
// source_ (see ws_bridge_handle_command below), so this object is never
// commanded directly and its own write_state() never fires in that case.
void WsBridgeSwitch::write_state(bool state) { this->publish_state(state); }

void WsBridgeSwitch::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_switch(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeSwitch::ws_bridge_declare() {
  switch_::Switch &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_switch(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_switch(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
