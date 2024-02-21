#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uartex/uartex_device.h"

namespace esphome {
namespace uartex {

class UARTExTextSensor : public text_sensor::TextSensor, public UARTExDevice
{
public:
    void dump_config() override;
    void set_template(std::function<optional<const char*>(const uint8_t *data, const uint16_t len)> &&f) { this->f_ = f; }

protected:
    void publish(const std::vector<uint8_t>& data) override;
    
protected:
    optional<std::function<optional<const char*>(const uint8_t *data, const uint16_t len)>> f_{};
};

}  // namespace uartex
}  // namespace esphome
