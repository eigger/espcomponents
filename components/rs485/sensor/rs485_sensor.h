#pragma once

#include "esphome/core/component.h"
#include "esphome/components/rs485/rs485.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace rs485 {

class RS485Sensor : public sensor::Sensor, public RS485Device {
  public:
    RS485Sensor() { device_name_ = &this->name_; }
    void dump_config() override;
    void publish(const uint8_t *data, const num_t len) override;
    bool publish(bool state) override { return false; }

    void set_state_num(state_num_t state_num) { this->conf_state_num_ = state_num; }
    void set_template(std::function<optional<float>(const uint8_t *data, const num_t len)> &&f) { this->f_ = f; }
    float get_setup_priority() const override { return setup_priority::DATA; }

  protected:
    optional<std::function<optional<float>(const uint8_t *data, const num_t len)>> f_{};
    optional<state_num_t> conf_state_num_{};
    
};

}  // namespace rs485
}  // namespace esphome
