#pragma once
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
    optional<float> get_state_number(const std::vector<uint8_t>& data) { return get_state_float("state_number", data); }
protected:
};

}  // namespace uartex
}  // namespace esphome
