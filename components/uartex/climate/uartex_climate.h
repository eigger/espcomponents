#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include <unordered_map>

namespace esphome {
namespace uartex {

class UARTExClimate : public climate::Climate, public UARTExDevice
{
public:
    UARTExClimate() {}
    void dump_config() override;
    void setup() override;
    void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
    void set_state_current_temperature(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_current_temperature_func_ = f; }
    void set_state_current_temperature(state_num_t state) { this->state_current_temperature_ = state; }
    void set_state_target_temperature(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_target_temperature_func_ = f; }
    void set_state_target_temperature(state_num_t state) { this->state_target_temperature_ = state; }
    void set_state_current_humidity(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_current_humidity_func_ = f; }
    void set_state_current_humidity(state_num_t state) { this->state_current_humidity_ = state; }
    void set_state_target_humidity(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_target_humidity_func_ = f; }
    void set_state_target_humidity(state_num_t state) { this->state_target_humidity_ = state; }
    void set_state_cool(state_t state) { this->state_mode_[climate::CLIMATE_MODE_COOL] = state; }
    void set_state_heat(state_t state) { this->state_mode_[climate::CLIMATE_MODE_HEAT] = state; }
    void set_state_fan_only(state_t state) { this->state_mode_[climate::CLIMATE_MODE_FAN_ONLY] = state; }
    void set_state_dry(state_t state) { this->state_mode_[climate::CLIMATE_MODE_DRY] = state; }
    void set_state_auto(state_t state) { this->state_mode_[climate::CLIMATE_MODE_AUTO] = state; }
    void set_state_fan_on(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_ON] = state; }
    void set_state_fan_off(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_OFF] = state; }
    void set_state_fan_auto(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_AUTO] = state; }
    void set_state_fan_low(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_LOW] = state; }
    void set_state_fan_medium(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_MEDIUM] = state; }
    void set_state_fan_high(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_HIGH] = state; }
    void set_state_fan_middle(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_MIDDLE] = state; }
    void set_state_fan_focus(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_FOCUS] = state; }
    void set_state_fan_diffuse(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_DIFFUSE] = state; }
    void set_state_fan_quiet(state_t state) { this->state_fan_mode_[climate::CLIMATE_FAN_QUIET] = state; }
    void set_state_swing_off(state_t state) { this->state_swing_mode_[climate::CLIMATE_SWING_OFF] = state; }
    void set_state_swing_both(state_t state) { this->state_swing_mode_[climate::CLIMATE_SWING_BOTH] = state; }
    void set_state_swing_vertical(state_t state) { this->state_swing_mode_[climate::CLIMATE_SWING_VERTICAL] = state; }
    void set_state_swing_horizontal(state_t state) { this->state_swing_mode_[climate::CLIMATE_SWING_HORIZONTAL] = state; }
    void set_state_preset_none(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_NONE] = state; }
    void set_state_preset_home(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_HOME] = state; }
    void set_state_preset_away(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_AWAY] = state; }
    void set_state_preset_boost(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_BOOST] = state; }
    void set_state_preset_comfort(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_COMFORT] = state; }
    void set_state_preset_eco(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_ECO] = state; }
    void set_state_preset_sleep(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_SLEEP] = state; }
    void set_state_preset_activity(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_ACTIVITY] = state; }
    void set_command_temperature(std::function<cmd_t(const float x)> f) { this->command_temperature_func_ = f; }
    void set_command_humidity(std::function<cmd_t(const float x)> f) { this->command_humidity_func_ = f; }
    void set_command_cool(std::function<cmd_t()> f) { this->command_mode_func_[climate::CLIMATE_MODE_COOL] = f; }
    void set_command_heat(std::function<cmd_t()> f) { this->command_mode_func_[climate::CLIMATE_MODE_HEAT] = f; }
    void set_command_fan_only(std::function<cmd_t()> f) { this->command_mode_func_[climate::CLIMATE_MODE_FAN_ONLY]= f; }
    void set_command_dry(std::function<cmd_t()> f) { this->command_mode_func_[climate::CLIMATE_MODE_DRY] = f; }
    void set_command_auto(std::function<cmd_t()> f) { this->command_mode_func_[climate::CLIMATE_MODE_AUTO] = f; }
    void set_command_cool(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_COOL] = cmd; }
    void set_command_heat(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_HEAT] = cmd; }
    void set_command_fan_only(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_FAN_ONLY] = cmd; }
    void set_command_dry(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_DRY] = cmd; }
    void set_command_auto(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_AUTO] = cmd; }
    void set_command_swing_off(std::function<cmd_t()> f) { this->command_swing_mode_func_[climate::CLIMATE_SWING_OFF]= f; }
    void set_command_swing_both(std::function<cmd_t()> f) { this->command_swing_mode_func_[climate::CLIMATE_SWING_BOTH] = f; }
    void set_command_swing_vertical(std::function<cmd_t()> f) { this->command_swing_mode_func_[climate::CLIMATE_SWING_VERTICAL] = f; }
    void set_command_swing_horizontal(std::function<cmd_t()> f) { this->command_swing_mode_func_[climate::CLIMATE_SWING_HORIZONTAL] = f; }
    void set_command_swing_off(cmd_t cmd) { this->command_swing_mode_[climate::CLIMATE_SWING_OFF]= cmd; }
    void set_command_swing_both(cmd_t cmd) { this->command_swing_mode_[climate::CLIMATE_SWING_BOTH] = cmd; }
    void set_command_swing_vertical(cmd_t cmd) { this->command_swing_mode_[climate::CLIMATE_SWING_VERTICAL] = cmd; }
    void set_command_swing_horizontal(cmd_t cmd) { this->command_swing_mode_[climate::CLIMATE_SWING_HORIZONTAL] = cmd; }
    void set_command_fan_on(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_ON] = f; }
    void set_command_fan_off(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_OFF] = f; }
    void set_command_fan_auto(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_AUTO] = f; }
    void set_command_fan_low(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_LOW] = f; }
    void set_command_fan_medium(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_MEDIUM] = f; }
    void set_command_fan_high(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_HIGH] = f; }
    void set_command_fan_middle(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_MIDDLE] = f; }
    void set_command_fan_focus(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_FOCUS] = f; }
    void set_command_fan_diffuse(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_DIFFUSE] = f; }
    void set_command_fan_quiet(std::function<cmd_t()> f) { this->command_fan_mode_func_[climate::CLIMATE_FAN_QUIET] = f; }
    void set_command_fan_on(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_ON] = cmd; }
    void set_command_fan_off(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_OFF] = cmd; }
    void set_command_fan_auto(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_AUTO] = cmd; }
    void set_command_fan_low(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_LOW] = cmd; }
    void set_command_fan_medium(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_MEDIUM] = cmd; }
    void set_command_fan_high(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_HIGH] = cmd; }
    void set_command_fan_middle(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_MIDDLE] = cmd; }
    void set_command_fan_focus(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_FOCUS] = cmd; }
    void set_command_fan_diffuse(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_DIFFUSE] = cmd; }
    void set_command_fan_quiet(cmd_t cmd) { this->command_fan_mode_[climate::CLIMATE_FAN_QUIET] = cmd; }
    void set_command_preset_none(std::function<cmd_t()> f) { this->command_preset_func_[climate::CLIMATE_PRESET_NONE] = f; }
    void set_command_preset_home(std::function<cmd_t()> f) { this->command_preset_func_[climate::CLIMATE_PRESET_HOME] = f; }
    void set_command_preset_away(std::function<cmd_t()> f) { this->command_preset_func_[climate::CLIMATE_PRESET_AWAY] = f; }
    void set_command_preset_boost(std::function<cmd_t()> f) { this->command_preset_func_[climate::CLIMATE_PRESET_BOOST] = f; }
    void set_command_preset_comfort(std::function<cmd_t()> f) { this->command_preset_func_[climate::CLIMATE_PRESET_COMFORT] = f; }
    void set_command_preset_eco(std::function<cmd_t()> f) { this->command_preset_func_[climate::CLIMATE_PRESET_ECO] = f; }
    void set_command_preset_sleep(std::function<cmd_t()> f) { this->command_preset_func_[climate::CLIMATE_PRESET_SLEEP] = f; }
    void set_command_preset_activity(std::function<cmd_t()> f) { this->command_preset_func_[climate::CLIMATE_PRESET_ACTIVITY] = f; }
    void set_command_preset_none(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_NONE] = cmd; }
    void set_command_preset_home(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_HOME] = cmd; }
    void set_command_preset_away(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_AWAY] = cmd; }
    void set_command_preset_boost(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_BOOST] = cmd; }
    void set_command_preset_comfort(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_COMFORT] = cmd; }
    void set_command_preset_eco(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_ECO] = cmd; }
    void set_command_preset_sleep(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_SLEEP] = cmd; }
    void set_command_preset_activity(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_ACTIVITY] = cmd; }

protected:
    void publish(const std::vector<uint8_t> &data) override;
    void control(const climate::ClimateCall &call) override;
    climate::ClimateTraits traits() override;

protected:
    std::unordered_map<climate::ClimateMode, optional<state_t>> state_mode_{};
    std::unordered_map<climate::ClimateSwingMode, optional<state_t>> state_swing_mode_{};
    std::unordered_map<climate::ClimateFanMode, optional<state_t>> state_fan_mode_{};
    std::unordered_map<climate::ClimatePreset, optional<state_t>> state_preset_{};
    std::unordered_map<climate::ClimateMode, cmd_t> command_mode_{};
    std::unordered_map<climate::ClimateMode, std::function<cmd_t()>> command_mode_func_{};
    std::unordered_map<climate::ClimateSwingMode, cmd_t> command_swing_mode_{};
    std::unordered_map<climate::ClimateSwingMode, std::function<cmd_t()>> command_swing_mode_func_{};
    std::unordered_map<climate::ClimateFanMode, cmd_t> command_fan_mode_{};
    std::unordered_map<climate::ClimateFanMode, std::function<cmd_t()>> command_fan_mode_func_{};
    std::unordered_map<climate::ClimatePreset, cmd_t> command_preset_{};
    std::unordered_map<climate::ClimatePreset, std::function<cmd_t()>> command_preset_func_{};
    sensor::Sensor *sensor_{nullptr};
    
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_current_temperature_func_{};
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_target_temperature_func_{};
    optional<state_num_t> state_current_temperature_{};
    optional<state_num_t> state_target_temperature_{};
    std::function<cmd_t(const float x)> command_temperature_func_{};
    cmd_t command_temperature_{};

    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_current_humidity_func_{};
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_target_humidity_func_{};
    optional<state_num_t> state_current_humidity_{};
    optional<state_num_t> state_target_humidity_{};
    std::function<cmd_t(const float x)> command_humidity_func_{};
    cmd_t command_humidity_{};

    
};

}  // namespace uartex
}  // namespace esphome
