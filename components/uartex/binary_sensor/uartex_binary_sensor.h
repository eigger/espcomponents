#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace uartex {

class UARTExBinarySensor : public UARTExDevice, public binary_sensor::BinarySensor
{
public:
    void dump_config() override;

protected:
    void setup() override;
    void publish(const bool state) override { publish_state(state); }
};

}  // namespace uartex
}  // namespace esphome
