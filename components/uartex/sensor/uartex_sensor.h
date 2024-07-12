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

protected:
    void publish(const std::vector<uint8_t>& data) override;
    state_num_t* get_state_number() { return get_state_num("state_number"); }
protected:
};

}  // namespace uartex
}  // namespace esphome
