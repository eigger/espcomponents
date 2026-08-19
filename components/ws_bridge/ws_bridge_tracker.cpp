#include "ws_bridge_tracker.h"
#include <cmath>
#include "esphome/core/log.h"
#include "ws_bridge.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.tracker";

void WsBridgeTracker::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Tracker '%s':", this->name_.c_str());
  ESP_LOGCONFIG(TAG, "  Unique ID: %s", this->unique_id_.c_str());
  LOG_UPDATE_INTERVAL(this);
}

void WsBridgeTracker::update() { this->publish_position_(); }

void WsBridgeTracker::ws_bridge_declare() {
  this->parent_->send_entity_declare(this->unique_id_, "device_tracker", this->name_, this->device_id_,
                                     this->device_name_, [this](JsonObject root) {
                                       if (!this->icon_.empty())
                                         root["icon"] = this->icon_;
                                     });
  this->publish_position_();
}

void WsBridgeTracker::publish_position_() {
  float lat = this->latitude_.value();
  float lon = this->longitude_.value();
  // No fix yet, or the source sensor has no state. Never send the coordinates
  // anyway: a NaN that serializes badly, or a zeroed pair, would put the
  // device at 0,0 — a spot in the Atlantic that Home Assistant can't tell
  // apart from a real reading. When report_unknown_ is true, send "unknown" so
  // HA marks the tracker unavailable; otherwise skip the update and leave the
  // last reported coordinates in place.
  if (std::isnan(lat) || std::isnan(lon)) {
    if (this->report_unknown_)
      this->parent_->send_state_string(this->unique_id_, "unknown");
    return;
  }
  this->parent_->send_state_object(this->unique_id_, [this, lat, lon](JsonObject value) {
    value["latitude"] = lat;
    value["longitude"] = lon;
    if (this->has_gps_accuracy_)
      value["gps_accuracy"] = this->gps_accuracy_.value();
  });
}

}  // namespace ws_bridge
}  // namespace esphome
