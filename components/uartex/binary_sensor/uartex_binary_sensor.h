#pragma once

#include "esphome/components/uartex/uartex.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace uartex {

class UartExBinarySensor : public UartExDevice, public binary_sensor::BinarySensor
{
public:
    UartExBinarySensor() { device_name_ = &this->name_; }
    void dump_config() override;
    void publish(const uint8_t *data, const num_t len) override;
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
