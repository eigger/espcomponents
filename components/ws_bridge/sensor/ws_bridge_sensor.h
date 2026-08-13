#pragma once
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeSensor : public sensor::Sensor, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror an existing sensor instead of expecting one's own state
  // pushed via lambdas. Metadata (device_class, unit_of_measurement, ...)
  // inherits from it field by field; anything set on this platform still wins.
  void set_source(sensor::Sensor *source) { this->source_ = source; }
  void set_accuracy_overridden(bool v) { this->accuracy_overridden_ = v; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;

 protected:
  sensor::Sensor *source_{nullptr};
  bool accuracy_overridden_{false};
};

}  // namespace ws_bridge
}  // namespace esphome
