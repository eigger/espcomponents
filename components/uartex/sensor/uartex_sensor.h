#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace uartex {

class UARTExSensor : public sensor::Sensor, public UARTExDevice
{
public:
    void dump_config() override;
    void set_state_num(state_num_t state_num) { this->state_num_map_["state_num"] = state;   }
    void set_template(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_func_map_["state_template"] = f; }

protected:
    void publish(const std::vector<uint8_t>& data) override;
    state_num_t* get_state_num() { return get_state_num("state_num"); }
protected:
};

}  // namespace uartex
}  // namespace esphome
