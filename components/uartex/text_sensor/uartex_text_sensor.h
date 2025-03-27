#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace uartex {

class UARTExTextSensor : public text_sensor::TextSensor, public UARTExDevice
{
public:
    void dump_config() override;

protected:
    void setup() override;
    void publish(const std::vector<uint8_t>& data) override;
    
protected:
};

}  // namespace uartex
}  // namespace esphome
