#include "ws_bridge_button.h"
#include "esphome/core/log.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.button";

void WsBridgeButton::dump_config() {
  LOG_BUTTON("", "WS Bridge Button", this);
  if (this->button_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->button_->get_name().str().c_str());
}

void WsBridgeButton::press_action() {
  if (this->button_ != nullptr)
    this->button_->press();
}

void WsBridgeButton::ws_bridge_handle_command(const WsCommand &command) { ws_handle_command_button(this, command); }

void WsBridgeButton::ws_bridge_declare() {
  // When wrapping, metadata comes from the wrapped button and anything written
  // on this platform overrides it field by field — same rule as every other
  // ws_bridge wrapper. Name resolution follows the same order: this entity's
  // own name, then the wrapped button's, then the unique_id.
  button::Button &src = this->button_ != nullptr ? *this->button_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_button(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
}

}  // namespace ws_bridge
}  // namespace esphome
