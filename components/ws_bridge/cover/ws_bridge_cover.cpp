#include "ws_bridge_cover.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.cover";

void WsBridgeCover::setup() { ws_subscribe_cover(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeCover::dump_config() {
  LOG_COVER("", "WS Bridge Cover", this);
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

cover::CoverTraits WsBridgeCover::get_traits() {
  if (this->source_ != nullptr)
    return this->source_->get_traits();
  cover::CoverTraits traits;
  traits.set_supports_position(true);
  traits.set_supports_stop(true);
  return traits;
}

// Only reached when NOT wrapping — see WsBridgeSwitch::write_state().
void WsBridgeCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    this->current_operation = cover::COVER_OPERATION_IDLE;
  } else if (call.get_position().has_value()) {
    this->position = *call.get_position();
    this->current_operation = cover::COVER_OPERATION_IDLE;
  }
  if (call.get_tilt().has_value())
    this->tilt = *call.get_tilt();
  this->publish_state();
}

void WsBridgeCover::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_cover(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeCover::ws_bridge_declare() {
  cover::Cover &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_cover(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_cover(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
