#pragma once

#include "esphome/components/bluetoothex/bluetoothex_device.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace bluetoothex {

class BluetoothExLightOutput : public light::LightOutput, public BluetoothExDevice
{
public:
    void dump_config() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override
    {
        publish_state(state);
        return true;
    }

    void set_light(light::LightState *light)
    {
        device_name_ = &light->get_name();
        light_ = light;
    }

    light::LightTraits get_traits() override
    {
        auto traits = light::LightTraits();
        traits.supports_color_capability(light::ColorCapability::BRIGHTNESS);
        return traits;
    }

    void write_state(light::LightState *state) override
    {
        bool binary;
        state->current_values_as_binary(&binary);
        if (binary == this->state_) return;
        push_tx_cmd(binary ? this->get_command_on() : this->get_command_off());
        this->state_ = binary;
    }

protected:
    bool state_{false};
    light::LightState *light_{nullptr};

    void publish_state(bool state);
};

}  // namespace bluetoothex
}  // namespace esphome
