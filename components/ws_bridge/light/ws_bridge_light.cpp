#include "ws_bridge_light.h"
#include "esphome/core/log.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.light";

void WsBridgeLight::setup() { ws_subscribe_light(this, this->light_); }

std::string WsBridgeLight::ha_name_() const {
  return ws_ha_name(*this->light_, this->name_, this->unique_id_);
}

void WsBridgeLight::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Light '%s'", this->ha_name_().c_str());
  ESP_LOGCONFIG(TAG, "  Unique ID: %s", this->unique_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->light_->get_name().str().c_str());
}

void WsBridgeLight::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_light(this->light_, command);
}

void WsBridgeLight::ws_bridge_declare() {
  ws_declare_light(this, *this->light_, this->ha_name_());
  ws_push_state_light(this, *this->light_);
}

}  // namespace ws_bridge
}  // namespace esphome
