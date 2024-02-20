#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace uartex {

class UARTExBinarySensor : public UARTExDevice, public binary_sensor::BinarySensor
{
public:
    void dump_config() override;
    bool publish(bool state) override
    {
        publish_state(state);
        return true;
    }

    float get_setup_priority() const override { return setup_priority::DATA; }

protected:
};

}  // namespace uartex
}  // namespace esphome
