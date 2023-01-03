#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace uartex {

class UARTExNumber : public number::Number, public UARTExDevice 
{
public:
    UARTExNumber() { device_name_ = &this->name_; }
    void dump_config() override;
    void setup() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override { return false; }
    void control(float value) override;
    void set_state_number(state_num_t state_number) { state_number_ = state_number; }
    void set_state_number(std::function<optional<float>(const uint8_t *data, const uint16_t len, const float state)> f) { state_number_func_ = f; }
    void set_command_number(std::function<cmd_t(const float x)> f) { command_number_func_ = f; }
protected:
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len, const float state)>> state_number_func_{};
    optional<std::function<cmd_t(const float x)>> command_number_func_{};
    optional<state_num_t> state_number_{};
    cmd_t command_number_{};
};

}  // namespace uartex
}  // namespace esphome