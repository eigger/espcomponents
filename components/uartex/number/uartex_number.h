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
    void set_state_number(state_num_t state) { this->state_number_ = state; }
    void set_state_number(std::function<optional<float>(const uint8_t *data, const uint16_t len, const float state)> f) { this->state_number_func_ = f; }
    void set_command_number(std::function<cmd_t(const float x)> f) { this->command_number_func_ = f; }

protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(float value) override;
    
protected:
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len, const float state)>> state_number_func_{};
    optional<std::function<cmd_t(const float x)>> command_number_func_{};
    optional<state_num_t> state_number_{};
    cmd_t command_number_{};
};

}  // namespace uartex
}  // namespace esphome