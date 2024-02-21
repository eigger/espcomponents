#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace uartex {

class UARTExLightOutput : public light::LightOutput, public UARTExDevice
{
public:
    void dump_config() override;
    void set_command_brightness(std::function<cmd_t(const float x)> f) { this->command_brightness_func_ = f; }

protected:
    void setup_state(light::LightState *state) override { this->light_state_ = state; }
    void publish(const bool state) override { publish_state(state); }
    void publish_state(bool state);
    light::LightTraits get_traits() override;
    void write_state(light::LightState *state) override;
    cmd_t* get_command_brightness();
protected:
    bool state_{false};
    int brightness_{0};
    light::LightState *light_state_{nullptr};
    optional<std::function<cmd_t(const float x)>> command_brightness_func_{};
    cmd_t command_brightness_{};
};

}  // namespace uartex
}  // namespace esphome
