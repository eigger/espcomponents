#include "ws_bridge_button.h"
#include "../ws_bridge.h"
#include "../ws_bridge_entity_json.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.button";

void WsBridgeButton::dump_config() { LOG_BUTTON("", "WS Bridge Button", this); }

void WsBridgeButton::ws_bridge_handle_command(const WsCommand &command) {
  if (command.action == "press") this->press();
}

void WsBridgeButton::ws_bridge_declare() {
  this->parent_->send_entity_declare(this->unique_id_, "button", this->get_name().str(), this->device_id_,
                                     this->device_name_,
                                     [this](JsonObject root) { add_common_entity_fields(root, *this); });
}

}  // namespace ws_bridge
}  // namespace esphome
