#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/bluetoothex/bluetoothex_device.h"

namespace esphome {
namespace bluetoothex {

class BluetoothExTextSensor : public text_sensor::TextSensor, public BluetoothExDevice
{
public:
    BluetoothExTextSensor() { device_name_ = &this->name_; }
    void dump_config() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override { return false; }

    //void set_template(std::function<optional<std::string>(const uint8_t *data, const uint16_t len)> &&f) { this->f_ = f; }
    void set_template(std::function<optional<const char*>(const uint8_t *data, const uint16_t len)> &&f) { this->f_ = f; }
    float get_setup_priority() const override { return setup_priority::DATA; }

protected:
    //optional<std::function<optional<std::string>(const uint8_t *data, const uint16_t len)>> f_{};
    optional<std::function<optional<const char*>(const uint8_t *data, const uint16_t len)>> f_{};
};

}  // namespace bluetoothex
}  // namespace esphome
