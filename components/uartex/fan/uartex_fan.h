#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace uartex {

class UARTExFan : public fan::Fan, public UARTExDevice
{
public:
    void dump_config() override;
    void set_speed_count(uint16_t count) { this->speed_count_ = count; }
    void set_preset_modes(std::initializer_list<const char *> presets) { this->preset_modes_ = presets; }
protected:
    fan::FanTraits get_traits() override;
    void publish(const std::vector<uint8_t>& data) override;
    void publish(const bool state) override;
    void control(const fan::FanCall& call) override;
    cmd_t* get_command_speed(const float x) { return get_command("command_speed", x); }
    cmd_t* get_command_preset(const std::string& str) { return get_command("command_preset", str); }
    optional<float> get_state_speed(const std::vector<uint8_t>& data) { return get_state_float("state_speed", data); }
    optional<std::string> get_state_preset(const std::vector<uint8_t>& data) { return get_state_str("state_preset", data); }
protected:
    uint16_t speed_count_{0};
    std::vector<const char *> preset_modes_{};
};

}  // namespace uartex
}  // namespace esphome
