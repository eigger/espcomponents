#pragma once
#include <string>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

// A Home Assistant device_tracker (GPS location) entity.
//
// Configured inside the `ws_bridge:` hub block rather than as a
// `platform: ws_bridge` under some ESPHome domain, because ESPHome has no
// device_tracker domain to hang one off. It still registers as a
// WsBridgeDevice, so it is re-declared automatically on connect and on every
// periodic re-announce, exactly like the real platform entities — which is the
// main thing this buys over declaring one by hand from a lambda.
class WsBridgeTracker : public PollingComponent, public WsBridgeDevice {
 public:
  void set_name(const std::string &name) { this->name_ = name; }
  void set_icon(const std::string &icon) { this->icon_ = icon; }
  template<typename V> void set_latitude(V v) { this->latitude_ = v; }
  template<typename V> void set_longitude(V v) { this->longitude_ = v; }
  template<typename V> void set_gps_accuracy(V v) {
    this->gps_accuracy_ = v;
    this->has_gps_accuracy_ = true;
  }
  void set_report_unknown(bool report_unknown) { this->report_unknown_ = report_unknown; }

  void dump_config() override;
  void update() override;
  void ws_bridge_declare() override;

 protected:
  void publish_position_();

  std::string name_;
  std::string icon_;
  TemplatableValue<float> latitude_{};
  TemplatableValue<float> longitude_{};
  TemplatableValue<float> gps_accuracy_{};
  bool has_gps_accuracy_{false};
  bool report_unknown_{true};
};

}  // namespace ws_bridge
}  // namespace esphome
