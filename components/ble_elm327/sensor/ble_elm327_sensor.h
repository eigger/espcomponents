#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_elm327/ble_elm327.h"

namespace esphome {
namespace ble_elm327 {

class BleElm327Sensor : public sensor::Sensor, public BleElm327Device {
 public:
  void dump_config() override;
  void publish_data(const std::vector<uint8_t> &data) override;
};

}  // namespace ble_elm327
}  // namespace esphome
