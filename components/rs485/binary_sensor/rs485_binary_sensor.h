#pragma once

#include "esphome/components/rs485/rs485.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace rs485 {

class RS485BinarySensor : public RS485Device, public binary_sensor::BinarySensor {
  public:
    RS485BinarySensor() { device_name_ = &this->name_; }
    void dump_config() override;
    void publish(const uint8_t *data, const num_t len) override;
    bool publish(bool state) override { publish_state(state); return true; }

    float get_setup_priority() const override { return setup_priority::DATA; }

  protected:
    
};

}  // namespace rs485
}  // namespace esphome
