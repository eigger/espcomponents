#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace uartex {

class UARTExFan : public fan::Fan, public UARTExDevice
{
public:
    UARTExFan() { this->device_name_ = &this->name_; }
    void dump_config() override;
    void setup() override;
    void set_speed_low(state_t state, cmd_t cmd)
    {
        this->state_speed_low_ = state;
        this->command_speed_low_ = cmd;
    }
    void set_speed_medium(state_t state, cmd_t cmd)
    {
        this->state_speed_medium_ = state;
        this->command_speed_medium_ = cmd;
    }
    void set_speed_high(state_t state, cmd_t cmd)
    {
        this->state_speed_high_ = state;
        this->command_speed_high_ = cmd;
    }
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override
    {
        this->state = state; 
        this->publish_state();
        return false; 
    }
    void control(const fan::FanCall &call) override;
protected:
    fan::FanTraits get_traits() override
    {
        fan::FanTraits traits{};
        if (speed_count_ > 0)
        {
            traits.set_speed(true);
            traits.set_supported_speed_count(speed_count_);
        }
        return traits;
    }
protected:
    int speed_count_{0};

    optional<state_t> state_speed_low_{};
    optional<state_t> state_speed_medium_{};
    optional<state_t> state_speed_high_{};
    optional<cmd_t> command_speed_low_{};
    optional<cmd_t> command_speed_medium_{};
    optional<cmd_t> command_speed_high_{};
};

}  // namespace uartex
}  // namespace esphome
