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
    void set_state_speed(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_func_map_["state_speed"] = f; }
    void set_command_speed(std::function<cmd_t(const float x)> f) { this->command_param_func_map_["command_speed"] = f; }

protected:
    fan::FanTraits get_traits() override;
    void publish(const std::vector<uint8_t>& data) override;
    void publish(const bool state) override;
    void control(const fan::FanCall &call) override;
    cmd_t* get_command_speed();
protected:
    uint16_t speed_count_{0};
};

}  // namespace uartex
}  // namespace esphome
