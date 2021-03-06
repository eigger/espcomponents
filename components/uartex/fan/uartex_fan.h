#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/fan/fan_state.h"

namespace esphome {
namespace uartex {

class UARTExFan : public UARTExDevice
{
public:
    UARTExFan(fan::FanState *fan) : fan_(fan) { this->device_name_ = &fan->get_name(); }
    void dump_config() override;
    void setup() override;
    void set_speed_low(hex_t state, cmd_hex_t command)
    {
        this->state_speed_low_ = state;
        this->command_speed_low_ = command;
    }
    void set_speed_medium(hex_t state, cmd_hex_t command)
    {
        this->state_speed_medium_ = state;
        this->command_speed_medium_ = command;
    }
    void set_speed_high(hex_t state, cmd_hex_t command)
    {
        this->state_speed_high_ = state;
        this->command_speed_high_ = command;
    }
    void perform();

    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override
    {
        publish_state(state);
        return !state;
    }

protected:
    fan::FanState *fan_;
    int speed_{0};
    bool support_speed_{false};
    bool state_{false};

    hex_t state_speed_low_{};
    hex_t state_speed_medium_{};
    hex_t state_speed_high_{};
    cmd_hex_t command_speed_low_{};
    cmd_hex_t command_speed_medium_{};
    cmd_hex_t command_speed_high_{};

    void publish_state(bool state);
    void publish_state(int speed);
};

}  // namespace uartex
}  // namespace esphome
