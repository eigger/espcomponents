#pragma once
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeSensor : public sensor::Sensor, public Component, public WsBridgeDevice {
 public:
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
};

}  // namespace ws_bridge
}  // namespace esphome
