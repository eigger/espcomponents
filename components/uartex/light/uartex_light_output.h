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
    bool publish(bool state) override
    {
        publish_state(state);
        return true;
    }

    void setup_state(light::LightState *state) override { this->light_state_ = state; }

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
        enqueue_tx_cmd(binary ? this->get_command_on() : this->get_command_off());
        this->state_ = binary;
    }

protected:
    bool state_{false};
    light::LightState *light_state_{nullptr};
    void publish_state(bool state);
};

}  // namespace uartex
}  // namespace esphome
