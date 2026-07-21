#pragma once
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeTextSensor : public text_sensor::TextSensor, public Component, public WsBridgeDevice {
 public:
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
};

}  // namespace ws_bridge
}  // namespace esphome
