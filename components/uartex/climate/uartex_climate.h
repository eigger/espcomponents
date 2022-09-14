#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace uartex {

class UARTExClimate : public climate::Climate, public UARTExDevice
{
public:
    UARTExClimate() { this->device_name_ = &this->name_; }
    void dump_config() override;
    void setup() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override { return false; }

    void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
    void set_state_current(std::function<optional<float>(const uint8_t *data, const num_t len)> f) { state_current_func_ = f; }
    void set_state_current(state_num_t state_current) { state_current_ = state_current; }
    void set_state_target(std::function<optional<float>(const uint8_t *data, const num_t len)> f) { state_target_func_ = f; }
    void set_state_target(state_num_t state_target) { state_target_ = state_target; }
    void set_state_auto(state_t state)
    {
        state_auto_ = state;
        supports_auto_ = true;
    }
    void set_state_cool(state_t state)
    {
        state_cool_ = state;
        supports_cool_ = true;
    }
    void set_state_heat(state_t state)
    {
        state_heat_ = state;
        supports_heat_ = true;
    }
    void set_state_away(state_t state)
    {
        state_away_ = state;
        supports_away_ = true;
    }

    void set_command_temperature(std::function<cmd_t(const float x)> f) { command_temperature_func_ = f; }
    void set_command_auto(cmd_t cmd)
    {
        command_auto_ = cmd;
        supports_auto_ = true;
    }
    void set_command_cool(cmd_t cmd)
    {
        command_cool_ = cmd;
        supports_cool_ = true;
    }
    void set_command_heat(cmd_t cmd)
    {
        command_heat_ = cmd;
        supports_heat_ = true;
    }
    void set_command_away(cmd_t cmd)
    {
        command_away_ = cmd;
        supports_away_ = true;
    }
    void set_command_home(cmd_t cmd) { command_home_ = cmd; }

protected:
    /// Override control to change settings of the climate device.
    void control(const climate::ClimateCall &call) override;
    /// Return the traits of this controller.
    climate::ClimateTraits traits() override;

    bool supports_auto_{false};
    bool supports_cool_{false};
    bool supports_heat_{false};
    bool supports_away_{false};

    /// Current temperature sensor
    sensor::Sensor *sensor_{nullptr};
    /// Template function current temperature
    optional<std::function<optional<float>(const uint8_t *data, const num_t len)>> state_current_func_{};
    /// Template function target temperature
    optional<std::function<optional<float>(const uint8_t *data, const num_t len)>> state_target_func_{};
    /// State current temperature
    optional<state_num_t> state_current_{};
    /// State target temperature
    optional<state_num_t> state_target_{};

    optional<state_t> state_away_{};
    optional<state_t> state_auto_{};
    optional<state_t> state_cool_{};
    optional<state_t> state_heat_{};

    std::function<cmd_t(const float x)> command_temperature_func_{};
    cmd_t command_temperature_{};
    optional<cmd_t> command_away_{};
    optional<cmd_t> command_home_{};
    optional<cmd_t> command_auto_{};
    optional<cmd_t> command_cool_{};
    optional<cmd_t> command_heat_{};
};

}  // namespace uartex
}  // namespace esphome
