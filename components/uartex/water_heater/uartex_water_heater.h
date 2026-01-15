#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/water_heater/water_heater.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace uartex {

class UARTExWaterHeater : public water_heater::WaterHeater, public UARTExDevice
{
public:
    UARTExWaterHeater() {}
    void dump_config() override;
    void setup() override;
    void set_sensor(sensor::Sensor* sensor) { this->sensor_ = sensor; }
    
    // Required by WaterHeater base class
    water_heater::WaterHeaterCallInternal make_call() override { return water_heater::WaterHeaterCallInternal(this); }
    
    // Resolve diamond inheritance ambiguity from Component
    //using water_heater::WaterHeater::set_component_source;

protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(const water_heater::WaterHeaterCall& call) override;
    water_heater::WaterHeaterTraits traits() override;

protected:
    sensor::Sensor* sensor_{nullptr};

    // Command getters
    cmd_t* get_command_temperature(const float x) { return get_command("command_temperature", x); }
    cmd_t* get_command_eco() { return get_command("command_eco"); }
    cmd_t* get_command_electric() { return get_command("command_electric"); }
    cmd_t* get_command_performance() { return get_command("command_performance"); }
    cmd_t* get_command_high_demand() { return get_command("command_high_demand"); }
    cmd_t* get_command_heat_pump() { return get_command("command_heat_pump"); }
    cmd_t* get_command_gas() { return get_command("command_gas"); }
    cmd_t* get_command_away_on() { return get_command("command_away_on"); }
    cmd_t* get_command_away_off() { return get_command("command_away_off"); }

    // State getters
    state_t* get_state_eco() { return UARTExDevice::get_state("state_eco"); }
    state_t* get_state_electric() { return UARTExDevice::get_state("state_electric"); }
    state_t* get_state_performance() { return UARTExDevice::get_state("state_performance"); }
    state_t* get_state_high_demand() { return UARTExDevice::get_state("state_high_demand"); }
    state_t* get_state_heat_pump() { return UARTExDevice::get_state("state_heat_pump"); }
    state_t* get_state_gas() { return UARTExDevice::get_state("state_gas"); }
    state_t* get_state_away_on() { return UARTExDevice::get_state("state_away_on"); }
    state_t* get_state_away_off() { return UARTExDevice::get_state("state_away_off"); }

    // Numeric state getters
    optional<float> get_state_temperature_current(const std::vector<uint8_t>& data) { return get_state_float("state_temperature_current", data); }
    optional<float> get_state_temperature_target(const std::vector<uint8_t>& data) { return get_state_float("state_temperature_target", data); }

    // Has state checks
    bool has_state_temperature_current() { return has_named_state("state_temperature_current"); }
    bool has_state_temperature_target() { return has_named_state("state_temperature_target"); }
};

}  // namespace uartex
}  // namespace esphome
