#pragma once

#include "esphome/components/bluetoothex/bluetoothex_device.h"
#include "esphome/components/fan/fan.h"

namespace esphome {
namespace bluetoothex {

class BluetoothExFan : public fan::Fan, public BluetoothExDevice
{
public:
    BluetoothExFan() { this->device_name_ = &this->name_; }
    void dump_config() override;
    void setup() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override
    {
        if (state == this->state) return false;
        this->state = state; 
        this->publish_state();
        return false; 
    }
    void control(const fan::FanCall &call) override;
    void set_speed_count(uint16_t count) { speed_count_ = count; }
    void set_state_speed(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { state_speed_func_ = f; }
    void set_command_speed(std::function<cmd_t(const float x)> f) { command_speed_func_ = f; }
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
    uint16_t speed_count_{0};
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_speed_func_{};
    optional<std::function<cmd_t(const float x)>> command_speed_func_{};
    cmd_t command_speed_{};
};

}  // namespace bluetoothex
}  // namespace esphome
