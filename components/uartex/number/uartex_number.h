#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace uartex {

class UARTExNumber : public number::Number, public UARTExDevice 
{
public:
    void dump_config() override;
    void setup() override;

protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(float value) override;
    state_num_t* get_state_number() { return get_state_num("state_number"); }
    cmd_t* get_command_number(const float x) { return get_command("command_number", x); }
    
protected:

};

}  // namespace uartex
}  // namespace esphome