#include "ws_bridge_update.h"
#include "esphome/core/log.h"
#include "../ws_bridge.h"
#include "../ws_bridge_entity_json.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.update";

void WsBridgeUpdate::setup() {
  this->update_->add_on_state_callback([this]() { this->send_state_(); });
}

std::string WsBridgeUpdate::ha_name_() const {
  if (!this->name_.empty())
    return this->name_;
  // http_request update with only `id:` gets name=id and internal: true —
  // that id (e.g. "ota_update") must not become the HA entity name.
  if (this->update_->has_own_name())
    return this->update_->get_name().str();
  return this->unique_id_;
}

void WsBridgeUpdate::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Update '%s'", this->ha_name_().c_str());
  ESP_LOGCONFIG(TAG, "  Unique ID: %s", this->unique_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->update_->get_name().str().c_str());
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
      this->unique_id_, "update", this->ha_name_(), this->device_id_, this->device_name_,
      [this](JsonObject root) {
        add_common_entity_fields(root, *this->update_);
        if (root["device_class"].isNull())
          root["device_class"] = "firmware";
      });
  if (this->update_->has_state())
    this->send_state_();
}

}  // namespace ws_bridge
}  // namespace esphome
