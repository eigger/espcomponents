#include "ws_bridge_update.h"
#include <array>
#include "esphome/core/log.h"
#include "../ws_bridge.h"
#include "../ws_bridge_entity_json.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.update";

void WsBridgeUpdate::setup() {
  this->update_->add_on_state_callback([this]() { this->send_state_(); });
}

void WsBridgeUpdate::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Update");
  ESP_LOGCONFIG(TAG, "  Unique ID: %s", this->unique_id_.c_str());
}

void WsBridgeUpdate::ws_bridge_handle_command(const WsCommand &command) {
  if (command.action == "install") {
    this->update_->perform();
  } else if (command.action == "check") {
    this->update_->check();
  }
}

void WsBridgeUpdate::fill_state_(JsonObject value) {
  const auto &info = this->update_->update_info;
  if (!info.current_version.empty())
    value["installed_version"] = info.current_version;
  if (!info.latest_version.empty())
    value["latest_version"] = info.latest_version;
  value["in_progress"] = this->update_->state == update::UPDATE_STATE_INSTALLING;
  if (info.has_progress)
    value["progress"] = static_cast<int>(info.progress);
  if (!info.title.empty())
    value["title"] = info.title;
  if (!info.summary.empty())
    value["summary"] = info.summary;
  if (!info.release_url.empty())
    value["release_url"] = info.release_url;
}

void WsBridgeUpdate::send_state_() {
  this->parent_->send_state_object(this->unique_id_, [this](JsonObject value) { this->fill_state_(value); });
}

void WsBridgeUpdate::ws_bridge_declare() {
  this->parent_->send_entity_declare(
      this->unique_id_, "update", this->update_->get_name().str(), this->device_id_, this->device_name_,
      [this](JsonObject root) {
        add_common_entity_fields(root, *this->update_);
        std::array<char, MAX_DEVICE_CLASS_LENGTH> dc_buf;
        const char *dc = this->update_->get_device_class_to(dc_buf);
        if (dc == nullptr || dc[0] == '\0')
          root["device_class"] = "firmware";
      });
  if (this->update_->has_state())
    this->send_state_();
}

}  // namespace ws_bridge
}  // namespace esphome
