#pragma once
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeTextSensor : public text_sensor::TextSensor, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror an existing text_sensor instead of expecting its own
  // state pushed via lambdas. Metadata inherits from it field by field;
  // anything set on this platform still wins.
  void set_source(text_sensor::TextSensor *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;

 protected:
  text_sensor::TextSensor *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
