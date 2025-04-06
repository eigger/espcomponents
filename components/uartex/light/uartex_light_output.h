#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

namespace esphome {
namespace uartex {

class UARTExLightOutput : public light::LightOutput, public UARTExDevice
{
public:
    void dump_config() override;

protected:
    void setup_state(light::LightState* state) override { this->light_state_ = state; }
    void publish(const std::vector<uint8_t>& data) override;
    void publish(const bool state) override { publish_state(state); }
    void publish_state(bool state);
    light::LightTraits get_traits() override;
    void write_state(light::LightState* state) override;
    cmd_t* get_command_brightness(const float x) { return get_command("command_brightness", x); }
    optional<float> get_state_brightness(const std::vector<uint8_t>& data) { return get_state_float("state_brightness", data); }
    bool has_state_brightness() { return has_state("state_brightness"); } 
protected:
    bool state_{false};
    int brightness_{0};
    light::LightState* light_state_{nullptr};
};

class UARTExLightState : public light::LightState
{
public:
    UARTExLightState(UARTExLightOutput *output) : LightState(output), Output(*output) {}
    UARTExLightOutput& Output;
};

}  // namespace uartex
}  // namespace esphome
