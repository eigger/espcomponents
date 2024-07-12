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
    void set_state_number(state_num_t state) { this->state_num_map_["state_number"] = state;  }
    void set_state_number(std::function<optional<float>(const uint8_t *data, const uint16_t len, const float state)> f) { this->state_func_map_["state_number"] = f; }
    void set_command_number(std::function<cmd_t(const float x)> f) { this->command_param_func_map_["command_number"] = f; }

protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(float value) override;
    state_num_t* get_state_number() { return get_state_num("state_number"); }
    cmd_t* get_command_number() { return get_command("command_number"); }
    
protected:

};

}  // namespace uartex
}  // namespace esphome