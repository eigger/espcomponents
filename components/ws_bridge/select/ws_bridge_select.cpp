#include "ws_bridge_select.h"
#include "../ws_bridge.h"
#include "../ws_bridge_entity_json.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.select";

void WsBridgeSelect::setup() {
  this->add_on_state_callback([this](size_t index) {
    this->parent_->send_state_string(this->unique_id_, this->option_at(index));
  });
}

void WsBridgeSelect::dump_config() { LOG_SELECT("", "WS Bridge Select", this); }

void WsBridgeSelect::control(const std::string &value) { this->publish_state(value); }

void WsBridgeSelect::ws_bridge_handle_command(const WsCommand &command) {
  if (command.action == "select_option" && command.has_value) {
    this->make_call().set_option(command.value_string).perform();
  }
}

void WsBridgeSelect::ws_bridge_declare() {
  this->parent_->send_entity_declare(
      this->unique_id_, "select", this->get_name().str(), this->device_id_, this->device_name_,
      [this](JsonObject root) {
        add_common_entity_fields(root, *this);
        JsonArray options = root["options"].to<JsonArray>();
        for (const char *option : this->traits.get_options()) options.add(option);
      });
  if (this->has_state()) this->parent_->send_state_string(this->unique_id_, this->current_option().str());
}

}  // namespace ws_bridge
}  // namespace esphome
