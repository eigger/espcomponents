#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/number/number.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace uartex {

class UARTExNumber : public number::Number, public UARTExDevice 
{
public:
    void dump_config() override;
    void setup() override;
    void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(float value) override;
    cmd_t* get_command_number(const float x) { return get_command("command_number", x); }
    optional<float> get_state_number(const std::vector<uint8_t>& data) { return get_state_float("state_number", data); }
    state_t* get_state_increment() { return get_state("state_increment"); }
    state_t* get_state_decrement() { return get_state("state_decrement"); }
    state_t* get_state_to_min() { return get_state("state_to_min"); }
    state_t* get_state_to_max() { return get_state("state_to_max"); }
protected:
    bool restore_value_{false};
    ESPPreferenceObject pref_;
};

}  // namespace uartex
}  // namespace esphome