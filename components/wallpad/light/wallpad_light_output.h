#pragma once

#include "esphome/components/wallpad/wallpad_device.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace wallpad {

class WallPadLightOutput : public light::LightOutput, public WallPadDevice
{
public:
    void dump_config() override;
    void publish(const uint8_t *data, const num_t len) override;
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
        send_command(binary ? this->get_command_on() : this->get_command_off());
        this->state_ = binary;
    }

protected:
    bool state_{false};
    light::LightState *light_{nullptr};

    void publish_state(bool state);
};

}  // namespace wallpad
}  // namespace esphome
