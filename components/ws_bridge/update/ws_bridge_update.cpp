#include "ws_bridge_update.h"
#include "esphome/core/log.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.update";

void WsBridgeUpdate::setup() { ws_subscribe_update(this, this->update_); }

std::string WsBridgeUpdate::ha_name_() const {
  return ws_ha_name(*this->update_, this->name_, this->unique_id_);
}

void WsBridgeUpdate::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Update '%s'", this->ha_name_().c_str());
  ESP_LOGCONFIG(TAG, "  Unique ID: %s", this->unique_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->update_->get_name().str().c_str());
}

void WsBridgeUpdate::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_update(this->update_, command);
}

void WsBridgeUpdate::ws_bridge_declare() {
  ws_declare_update(this, *this->update_, this->ha_name_());
  ws_push_state_update(this, *this->update_);
}

}  // namespace ws_bridge
}  // namespace esphome
