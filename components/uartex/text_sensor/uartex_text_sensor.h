#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uartex/uartex_device.h"

namespace esphome {
namespace uartex {

class UARTExTextSensor : public text_sensor::TextSensor, public UARTExDevice
{
public:
    UARTExTextSensor() { device_name_ = &this->name_; }
    void dump_config() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override { return false; }

    //void set_template(std::function<optional<std::string>(const uint8_t *data, const uint16_t len)> &&f) { this->f_ = f; }
    void set_template(std::function<optional<char*>(const uint8_t *data, const uint16_t len)> &&f) { this->f_ = f; }
    float get_setup_priority() const override { return setup_priority::DATA; }

protected:
    //optional<std::function<optional<std::string>(const uint8_t *data, const uint16_t len)>> f_{};
    optional<std::function<optional<char*>(const uint8_t *data, const uint16_t len)>> f_{};
};

}  // namespace uartex
}  // namespace esphome
