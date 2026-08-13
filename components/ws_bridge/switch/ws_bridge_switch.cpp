#include "ws_bridge_switch.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.switch";

void WsBridgeSwitch::setup() { ws_subscribe_switch(this, this); }

void WsBridgeSwitch::dump_config() { LOG_SWITCH("", "WS Bridge Switch", this); }

void WsBridgeSwitch::write_state(bool state) { this->publish_state(state); }

void WsBridgeSwitch::ws_bridge_handle_command(const WsCommand &command) { ws_handle_command_switch(this, command); }

void WsBridgeSwitch::ws_bridge_declare() {
  ws_declare_switch(this, *this, this, this->get_name().str());
  ws_push_state_switch(this, *this);
}

}  // namespace ws_bridge
}  // namespace esphome
