#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace uartex {

class UARTExClimate : public climate::Climate, public UARTExDevice
{
public:
    UARTExClimate() {}
    void dump_config() override;
    void setup() override;
    void set_sensor(sensor::Sensor* sensor) { this->sensor_ = sensor; }
    void set_custom_fan_modes(std::initializer_list<const char *> modes) { this->custom_fan_modes_ = modes; }
    void set_custom_preset_modes(std::initializer_list<const char *> modes) { this->custom_preset_modes_ = modes; }

protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(const climate::ClimateCall& call) override;
    climate::ClimateTraits traits() override;
    climate::ClimateAction mode_to_action(climate::ClimateMode mode);
protected:
    sensor::Sensor* sensor_{nullptr};

    cmd_t* get_command_temperature(const float x) { return get_command("command_temperature", x); }
    cmd_t* get_command_humidity(const float x) { return get_command("command_humidity", x); }
    cmd_t* get_command_cool() { return get_command("command_cool"); }
    cmd_t* get_command_heat() { return get_command("command_heat"); }
    cmd_t* get_command_fan_only() { return get_command("command_fan_only"); }
    cmd_t* get_command_dry() { return get_command("command_dry"); }
    cmd_t* get_command_auto() { return get_command("command_auto"); }
   
    cmd_t* get_command_swing_off() { return get_command("command_swing_off"); }
    cmd_t* get_command_swing_both() { return get_command("command_swing_both"); }
    cmd_t* get_command_swing_vertical() { return get_command("command_swing_vertical"); }
    cmd_t* get_command_swing_horizontal() { return get_command("command_swing_horizontal"); }

    cmd_t* get_command_fan_on() { return get_command("command_fan_on"); }
    cmd_t* get_command_fan_off() { return get_command("command_fan_off"); }
    cmd_t* get_command_fan_auto() { return get_command("command_fan_auto"); }
    cmd_t* get_command_fan_low() { return get_command("command_fan_low"); }
    cmd_t* get_command_fan_medium() { return get_command("command_fan_medium"); }
    cmd_t* get_command_fan_high() { return get_command("command_fan_high"); }
    cmd_t* get_command_fan_middle() { return get_command("command_fan_middle"); }
    cmd_t* get_command_fan_focus() { return get_command("command_fan_focus"); }
    cmd_t* get_command_fan_diffuse() { return get_command("command_fan_diffuse"); }
    cmd_t* get_command_fan_quiet() { return get_command("command_fan_quiet"); }
   
    cmd_t* get_command_preset_none() { return get_command("command_preset_none"); }
    cmd_t* get_command_preset_home() { return get_command("command_preset_home"); }
    cmd_t* get_command_preset_away() { return get_command("command_preset_away"); }
    cmd_t* get_command_preset_boost() { return get_command("command_preset_boost"); }
    cmd_t* get_command_preset_comfort() { return get_command("command_preset_comfort"); }
    cmd_t* get_command_preset_eco() { return get_command("command_preset_eco"); }
    cmd_t* get_command_preset_sleep() { return get_command("command_preset_sleep"); }
    cmd_t* get_command_preset_activity() { return get_command("command_preset_activity"); }

    cmd_t* get_command_custom_fan(const std::string& str) { return get_command("command_custom_fan", str); }
    cmd_t* get_command_custom_preset(const std::string& str) { return get_command("command_custom_preset", str); }

    state_t* get_state_cool() { return get_state("state_cool"); }
    state_t* get_state_heat() { return get_state("state_heat"); }
    state_t* get_state_fan_only() { return get_state("state_fan_only"); }
    state_t* get_state_dry() { return get_state("state_dry"); }
    state_t* get_state_auto() { return get_state("state_auto"); }

    state_t* get_state_action_cooling() { return get_state("state_action_cooling"); }
    state_t* get_state_action_heating() { return get_state("state_action_heating"); }
    state_t* get_state_action_idle() { return get_state("state_action_idle"); }
    state_t* get_state_action_drying() { return get_state("state_action_drying"); }
    state_t* get_state_action_fan() { return get_state("state_action_fan"); }

    state_t* get_state_fan_on() { return get_state("state_fan_on"); }
    state_t* get_state_fan_off() { return get_state("state_fan_off"); }
    state_t* get_state_fan_auto() { return get_state("state_fan_auto"); }
    state_t* get_state_fan_low() { return get_state("state_fan_low"); }
    state_t* get_state_fan_medium() { return get_state("state_fan_medium"); }
    state_t* get_state_fan_high() { return get_state("state_fan_high"); }
    state_t* get_state_fan_middle() { return get_state("state_fan_middle"); }
    state_t* get_state_fan_focus() { return get_state("state_fan_focus"); }
    state_t* get_state_fan_diffuse() { return get_state("state_fan_diffuse"); }
    state_t* get_state_fan_quiet() { return get_state("state_fan_quiet"); }

    state_t* get_state_swing_off() { return get_state("state_swing_off"); }
    state_t* get_state_swing_both() { return get_state("state_swing_both"); }
    state_t* get_state_swing_vertical() { return get_state("state_swing_vertical"); }
    state_t* get_state_swing_horizontal() { return get_state("state_swing_horizontal"); }

    state_t* get_state_preset_none() { return get_state("state_preset_none"); }
    state_t* get_state_preset_home() { return get_state("state_preset_home"); }
    state_t* get_state_preset_away() { return get_state("state_preset_away"); }
    state_t* get_state_preset_boost() { return get_state("state_preset_boost"); }
    state_t* get_state_preset_comfort() { return get_state("state_preset_comfort"); }
    state_t* get_state_preset_eco() { return get_state("state_preset_eco"); }
    state_t* get_state_preset_sleep() { return get_state("state_preset_sleep"); }
    state_t* get_state_preset_activity() { return get_state("state_preset_activity"); }

    optional<float> get_state_temperature_current(const std::vector<uint8_t>& data) { return get_state_float("state_temperature_current", data); }
    optional<float> get_state_temperature_target(const std::vector<uint8_t>& data) { return get_state_float("state_temperature_target", data); }
    optional<float> get_state_humidity_current(const std::vector<uint8_t>& data) { return get_state_float("state_humidity_current", data); }
    optional<float> get_state_humidity_target(const std::vector<uint8_t>& data) { return get_state_float("state_humidity_target", data); }

    optional<std::string> get_state_custom_fan(const std::vector<uint8_t>& data) { return get_state_str("state_custom_fan", data); }
    optional<std::string> get_state_custom_preset(const std::vector<uint8_t>& data) { return get_state_str("state_custom_preset", data); }

    bool has_state_temperature_current() { return has_named_state("state_temperature_current"); } 
    bool has_state_temperature_target() { return has_named_state("state_temperature_target"); } 
    bool has_state_humidity_current() { return has_named_state("state_humidity_current"); } 
    bool has_state_humidity_target() { return has_named_state("state_humidity_target"); } 

    std::vector<const char *> custom_fan_modes_{};
    std::vector<const char *> custom_preset_modes_{};
};

}  // namespace uartex
}  // namespace esphome
