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
    void set_state_current(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_current_func_ = f; }
    void set_state_current(state_num_t state) { this->state_current_ = state; }
    void set_state_target(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_target_func_ = f; }
    void set_state_target(state_num_t state) { this->state_target_ = state; }
    void set_state_cool(state_t state) { this->state_mode_[climate::CLIMATE_MODE_COOL] = state; set_supported_mode(climate::CLIMATE_MODE_COOL); }
    void set_state_heat(state_t state) { this->state_mode_[climate::CLIMATE_MODE_HEAT] = state; set_supported_mode(climate::CLIMATE_MODE_HEAT); }
    void set_state_fan_only(state_t state) { this->state_mode_[climate::CLIMATE_MODE_FAN_ONLY] = state; set_supported_mode(climate::CLIMATE_MODE_FAN_ONLY); }
    void set_state_dry(state_t state) { this->state_mode_[climate::CLIMATE_MODE_DRY] = state; set_supported_mode(climate::CLIMATE_MODE_DRY); }
    void set_state_auto(state_t state) { this->state_mode_[climate::CLIMATE_MODE_AUTO] = state; set_supported_mode(climate::CLIMATE_MODE_AUTO); }
    void set_state_swing_off(state_t state) { this->state_swing_mode_[climate::CLIMATE_SWING_OFF] = state; set_supported_swing_mode(climate::CLIMATE_SWING_OFF); }
    void set_state_swing_both(state_t state) { this->state_swing_mode_[climate::CLIMATE_SWING_BOTH] = state; set_supported_swing_mode(climate::CLIMATE_SWING_BOTH); }
    void set_state_swing_vertical(state_t state) { this->state_swing_mode_[climate::CLIMATE_SWING_VERTICAL] = state; set_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL); }
    void set_state_swing_horizontal(state_t state) { this->state_swing_mode_[climate::CLIMATE_SWING_HORIZONTAL] = state; set_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL); }
    void set_state_preset_none(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_NONE] = state; set_supported_preset(climate::CLIMATE_PRESET_NONE); }
    void set_state_preset_home(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_HOME] = state; set_supported_preset(climate::CLIMATE_PRESET_HOME); }
    void set_state_preset_away(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_AWAY] = state; set_supported_preset(climate::CLIMATE_PRESET_AWAY); }
    void set_state_preset_boost(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_BOOST] = state; set_supported_preset(climate::CLIMATE_PRESET_BOOST); }
    void set_state_preset_comfort(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_COMFORT] = state; set_supported_preset(climate::CLIMATE_PRESET_COMFORT); }
    void set_state_preset_eco(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_ECO] = state; set_supported_preset(climate::CLIMATE_PRESET_ECO); }
    void set_state_preset_sleep(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_SLEEP] = state; set_supported_preset(climate::CLIMATE_PRESET_SLEEP); }
    void set_state_preset_activity(state_t state) { this->state_preset_[climate::CLIMATE_PRESET_ACTIVITY] = state; set_supported_preset(climate::CLIMATE_PRESET_ACTIVITY); }
    void set_command_temperature(std::function<cmd_t(const float x, const uint8_t mode, const uint8_t preset)> f) { this->command_temperature_func_ = f; }
    void set_command_cool(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_COOL] = cmd; set_supported_mode(climate::CLIMATE_MODE_COOL); }
    void set_command_heat(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_HEAT] = cmd; set_supported_mode(climate::CLIMATE_MODE_HEAT); }
    void set_command_fan_only(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_FAN_ONLY] = cmd; set_supported_mode(climate::CLIMATE_MODE_FAN_ONLY); }
    void set_command_dry(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_DRY] = cmd; set_supported_mode(climate::CLIMATE_MODE_DRY); }
    void set_command_auto(cmd_t cmd) { this->command_mode_[climate::CLIMATE_MODE_AUTO] = cmd; set_supported_mode(climate::CLIMATE_MODE_AUTO); }
    void set_command_swing_off(cmd_t cmd) { this->command_swing_mode_[climate::CLIMATE_SWING_OFF]= cmd; set_supported_swing_mode(climate::CLIMATE_SWING_OFF); }
    void set_command_swing_both(cmd_t cmd) { this->command_swing_mode_[climate::CLIMATE_SWING_BOTH] = cmd; set_supported_swing_mode(climate::CLIMATE_SWING_BOTH); }
    void set_command_swing_vertical(cmd_t cmd) { this->command_swing_mode_[climate::CLIMATE_SWING_VERTICAL] = cmd; set_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL); }
    void set_command_swing_horizontal(cmd_t cmd) { this->command_swing_mode_[climate::CLIMATE_SWING_HORIZONTAL] = cmd; set_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL); }
    void set_command_preset_none(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_NONE] = cmd; set_supported_preset(climate::CLIMATE_PRESET_NONE); }
    void set_command_preset_home(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_HOME] = cmd; set_supported_preset(climate::CLIMATE_PRESET_HOME); }
    void set_command_preset_away(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_AWAY] = cmd; set_supported_preset(climate::CLIMATE_PRESET_AWAY); }
    void set_command_preset_boost(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_BOOST] = cmd; set_supported_preset(climate::CLIMATE_PRESET_BOOST); }
    void set_command_preset_comfort(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_COMFORT] = cmd; set_supported_preset(climate::CLIMATE_PRESET_COMFORT); }
    void set_command_preset_eco(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_ECO] = cmd; set_supported_preset(climate::CLIMATE_PRESET_ECO); }
    void set_command_preset_sleep(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_SLEEP] = cmd; set_supported_preset(climate::CLIMATE_PRESET_SLEEP); }
    void set_command_preset_activity(cmd_t cmd) { this->command_preset_[climate::CLIMATE_PRESET_ACTIVITY] = cmd; set_supported_preset(climate::CLIMATE_PRESET_ACTIVITY); }

protected:
    void set_supported_mode(climate::ClimateMode mode) { if (std::find(supported_mode_.begin(), supported_mode_.end(), mode) == supported_mode_.end()) supported_mode_.push_back(mode); }
    void set_supported_swing_mode(climate::ClimateSwingMode mode) { if (std::find(supported_swing_mode_.begin(), supported_swing_mode_.end(), mode) == supported_swing_mode_.end()) supported_swing_mode_.push_back(mode); }
    void set_supported_preset(climate::ClimatePreset preset) { if (std::find(supported_preset_.begin(), supported_preset_.end(), preset) == supported_preset_.end()) supported_preset_.push_back(preset); }
    void publish(const std::vector<uint8_t>& data) override;
    void control(const climate::ClimateCall &call) override;
    climate::ClimateTraits traits() override;

protected:
    std::vector<climate::ClimateMode> supported_mode_{};
    std::vector<climate::ClimateSwingMode> supported_swing_mode_{};
    std::vector<climate::ClimatePreset> supported_preset_{};
    std::unordered_map<climate::ClimateMode, optional<state_t>> state_mode_{};
    std::unordered_map<climate::ClimateSwingMode, optional<state_t>> state_swing_mode_{};
    std::unordered_map<climate::ClimatePreset, optional<state_t>> state_preset_{};
    std::unordered_map<climate::ClimateMode, optional<cmd_t>> command_mode_{};
    std::unordered_map<climate::ClimateSwingMode, optional<cmd_t>> command_swing_mode_{};
    std::unordered_map<climate::ClimatePreset, optional<cmd_t>> command_preset_{};
    sensor::Sensor *sensor_{nullptr};
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_current_func_{};
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_target_func_{};
    optional<state_num_t> state_current_{};
    optional<state_num_t> state_target_{};
    std::function<cmd_t(const float x, const uint8_t mode, const uint8_t preset)> command_temperature_func_{};
    cmd_t command_temperature_{};
};

}  // namespace uartex
}  // namespace esphome
