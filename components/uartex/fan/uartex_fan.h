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
    void set_state_speed(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_speed_func_ = f; }
    void set_command_speed(std::function<cmd_t(const float x)> f) { this->command_speed_func_ = f; }

protected:
    fan::FanTraits get_traits() override;
    void publish(const std::vector<uint8_t>& data) override;
    void publish(const bool state) override;
    void control(const fan::FanCall &call) override;
    cmd_t* get_command_speed();
protected:
    uint16_t speed_count_{0};
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_speed_func_{};
    optional<std::function<cmd_t(const float x)>> command_speed_func_{};
    cmd_t command_speed_{};
};

}  // namespace uartex
}  // namespace esphome
