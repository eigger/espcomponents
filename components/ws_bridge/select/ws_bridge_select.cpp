#include "ws_bridge_select.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.select";

void WsBridgeSelect::setup() { ws_subscribe_select(this, this); }

void WsBridgeSelect::dump_config() { LOG_SELECT("", "WS Bridge Select", this); }

void WsBridgeSelect::control(const std::string &value) { this->publish_state(value); }

void WsBridgeSelect::ws_bridge_handle_command(const WsCommand &command) { ws_handle_command_select(this, command); }

void WsBridgeSelect::ws_bridge_declare() {
  ws_declare_select(this, *this, this, this->get_name().str());
  ws_push_state_select(this, *this);
}

}  // namespace ws_bridge
}  // namespace esphome
