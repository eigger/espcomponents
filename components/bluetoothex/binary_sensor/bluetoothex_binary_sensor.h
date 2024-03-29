#pragma once

#include "esphome/components/bluetoothex/bluetoothex_device.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace bluetoothex {

class BluetoothExBinarySensor : public BluetoothExDevice, public binary_sensor::BinarySensor
{
public:
    BluetoothExBinarySensor() { device_name_ = &this->name_; }
    void dump_config() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override
    {
        publish_state(state);
        return true;
    }

    float get_setup_priority() const override { return setup_priority::DATA; }

protected:
};

}  // namespace bluetoothex
}  // namespace esphome
