#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace uartex {

class UARTExLightOutput : public light::LightOutput, public UARTExDevice
{
public:
    void dump_config() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override { publish_state(state); return true; }

    void setup_state(light::LightState *state) override { this->light_state_ = state; }
    void set_command_brightness(std::function<cmd_t(const float x)> f) { this->command_brightness_func_ = f; }
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
        if (binary != this->state_)
        {
            enqueue_tx_cmd(binary ? get_command_on() : get_command_off());
            this->state_ = binary;
        }
        if (this->command_brightness_func_.has_value())
        {
            float brightness;
            state->current_values_as_brightness(&brightness);
            if ((int)brightness != this->brightness_)
            {
                this->command_brightness_ = (*this->command_brightness_func_)(brightness);
                enqueue_tx_cmd(&this->command_brightness_);
                this->brightness_ = (int)brightness;
            }

        }
    }

protected:
    bool state_{false};
    int brightness_{0};
    light::LightState *light_state_{nullptr};
    void publish_state(bool state);
    optional<std::function<cmd_t(const float x)>> command_brightness_func_{};
    cmd_t command_brightness_{};
};

}  // namespace uartex
}  // namespace esphome
