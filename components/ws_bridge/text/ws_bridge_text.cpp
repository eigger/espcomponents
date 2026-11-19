#include "ws_bridge_text.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.text";

void WsBridgeText::setup() { ws_subscribe_text(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeText::dump_config() {
  LOG_TEXT("", "WS Bridge Text", this);
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", ws_entity_name(*this->source_).c_str());
}

// Only reached when NOT wrapping — see WsBridgeSwitch::write_state().
void WsBridgeText::control(const std::string &value) { this->publish_state(value); }

void WsBridgeText::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_text(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeText::ws_bridge_declare() {
  text::Text &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? ws_entity_name(*this) : "";
  ws_declare_text(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_text(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
