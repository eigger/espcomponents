#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace uartex {

class UARTExLightOutput : public light::LightOutput, public UARTExDevice
{
public:
    void dump_config() override;
    void set_state_brightness(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_func_map_["state_brightness"] = f; }
    void set_command_brightness(std::function<cmd_t(const float x)> f) { this->command_param_func_map_["command_brightness"] = f;}

protected:
    void setup_state(light::LightState *state) override { this->light_state_ = state; }
    void publish(const std::vector<uint8_t>& data) override;
    void publish(const bool state) override { publish_state(state); }
    void publish_state(bool state);
    light::LightTraits get_traits() override;
    void write_state(light::LightState *state) override;
    cmd_t* get_command_brightness(const float x);
protected:
    bool state_{false};
    int brightness_{0};
    light::LightState *light_state_{nullptr};
};

}  // namespace uartex
}  // namespace esphome
