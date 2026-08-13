#include "ws_bridge_number.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.number";

void WsBridgeNumber::setup() { ws_subscribe_number(this, this); }

void WsBridgeNumber::dump_config() { LOG_NUMBER("", "WS Bridge Number", this); }

void WsBridgeNumber::control(float value) { this->publish_state(value); }

void WsBridgeNumber::ws_bridge_handle_command(const WsCommand &command) { ws_handle_command_number(this, command); }

void WsBridgeNumber::ws_bridge_declare() {
  ws_declare_number(this, *this, this, this->get_name().str());
  ws_push_state_number(this, *this);
}

}  // namespace ws_bridge
}  // namespace esphome
