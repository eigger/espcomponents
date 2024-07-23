#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace botem_csm505 {

class BotemCSM505 : public number::Number, public uartex::UARTExDevice 
{
public:
    void dump_config() override;
    void setup() override;

protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(float value) override;

protected:

};
} // namespace botem_csm505
} // namespace esphome