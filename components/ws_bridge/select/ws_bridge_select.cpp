#include "ws_bridge_select.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.select";

void WsBridgeSelect::setup() { ws_subscribe_select(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeSelect::dump_config() {
  LOG_SELECT("", "WS Bridge Select", this);
  if (this->source_ != nullptr) ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

// Only reached when NOT wrapping — see WsBridgeSwitch::write_state().
void WsBridgeSelect::control(const std::string &value) { this->publish_state(value); }

void WsBridgeSelect::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_select(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeSelect::ws_bridge_declare() {
  select::Select &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_select(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_select(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
