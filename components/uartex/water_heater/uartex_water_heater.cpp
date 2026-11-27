#include "uartex_water_heater.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.water_heater";

void UARTExWaterHeater::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", uartex_entity_name(*this).c_str());
    log_config(TAG, "State Temperature Current", get_state_num("state_temperature_current"));
    log_config(TAG, "State Temperature Target", get_state_num("state_temperature_target"));
    log_config(TAG, "State Eco", get_state_eco());
    log_config(TAG, "State Electric", get_state_electric());
    log_config(TAG, "State Performance", get_state_performance());
    log_config(TAG, "State High Demand", get_state_high_demand());
    log_config(TAG, "State Heat Pump", get_state_heat_pump());
    log_config(TAG, "State Gas", get_state_gas());
    log_config(TAG, "State Away On", get_state_away_on());
    log_config(TAG, "State Away Off", get_state_away_off());
    log_config(TAG, "State On", get_state_on());
    log_config(TAG, "State Off", get_state_off());
    log_config(TAG, "Command On", get_command_on());
    log_config(TAG, "Command Off", get_command_off());
    log_config(TAG, "Command Eco", get_command_eco());
    log_config(TAG, "Command Electric", get_command_electric());
    log_config(TAG, "Command Performance", get_command_performance());
    log_config(TAG, "Command High Demand", get_command_high_demand());
    log_config(TAG, "Command Heat Pump", get_command_heat_pump());
    log_config(TAG, "Command Gas", get_command_gas());
    log_config(TAG, "Command Away On", get_command_away_on());
    log_config(TAG, "Command Away Off", get_command_away_off());
    uartex_dump_config(TAG);
#endif
}

water_heater::WaterHeaterTraits UARTExWaterHeater::traits()
{
    auto traits = water_heater::WaterHeaterTraits();
    
    if (this->sensor_ != nullptr || has_state_temperature_current())
    {
        traits.set_supports_current_temperature(true);
    }
    
    // Add supported modes based on configured states/commands
    water_heater::WaterHeaterModeMask supported_modes;
    if (get_command_off() || get_state_off()) supported_modes.insert(water_heater::WATER_HEATER_MODE_OFF);
    if (get_command_eco() || get_state_eco()) supported_modes.insert(water_heater::WATER_HEATER_MODE_ECO);
    if (get_command_electric() || get_state_electric()) supported_modes.insert(water_heater::WATER_HEATER_MODE_ELECTRIC);
    if (get_command_performance() || get_state_performance()) supported_modes.insert(water_heater::WATER_HEATER_MODE_PERFORMANCE);
    if (get_command_high_demand() || get_state_high_demand()) supported_modes.insert(water_heater::WATER_HEATER_MODE_HIGH_DEMAND);
    if (get_command_heat_pump() || get_state_heat_pump()) supported_modes.insert(water_heater::WATER_HEATER_MODE_HEAT_PUMP);
    if (get_command_gas() || get_state_gas()) supported_modes.insert(water_heater::WATER_HEATER_MODE_GAS);
    traits.set_supported_modes(supported_modes);
    
    // Away mode support
    if (get_command_away_on() || get_command_away_off() || get_state_away_on() || get_state_away_off())
    {
        traits.set_supports_away_mode(true);
    }
    
    // On/Off support
    if (get_command_on() || get_state_on())
    {
        traits.add_feature_flags(water_heater::WATER_HEATER_SUPPORTS_ON_OFF);
    }
    
    return traits;
}

void UARTExWaterHeater::setup()
{
    this->target_temperature_ = NAN;
    if (this->sensor_)
    {
        this->sensor_->add_on_state_callback([this](float state)
        {
            this->set_current_temperature(state);
            publish_state();
        });
        this->set_current_temperature(this->sensor_->state);
    }
    else
    {
        this->set_current_temperature(NAN);
    }
}

void UARTExWaterHeater::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    
    // Mode detection (a non-OFF mode implies ON; state_off clears it, an explicit state_on is applied after)
    if (verify_state(data, get_state_off()))
    {
        this->set_mode_(water_heater::WATER_HEATER_MODE_OFF);
        this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, false);
        changed = true;
    }
    else if (verify_state(data, get_state_eco()))
    {
        this->set_mode_(water_heater::WATER_HEATER_MODE_ECO);
        this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, true);
        changed = true;
    }
    else if (verify_state(data, get_state_electric()))
    {
        this->set_mode_(water_heater::WATER_HEATER_MODE_ELECTRIC);
        this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, true);
        changed = true;
    }
    else if (verify_state(data, get_state_performance()))
    {
        this->set_mode_(water_heater::WATER_HEATER_MODE_PERFORMANCE);
        this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, true);
        changed = true;
    }
    else if (verify_state(data, get_state_high_demand()))
    {
        this->set_mode_(water_heater::WATER_HEATER_MODE_HIGH_DEMAND);
        this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, true);
        changed = true;
    }
    else if (verify_state(data, get_state_heat_pump()))
    {
        this->set_mode_(water_heater::WATER_HEATER_MODE_HEAT_PUMP);
        this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, true);
        changed = true;
    }
    else if (verify_state(data, get_state_gas()))
    {
        this->set_mode_(water_heater::WATER_HEATER_MODE_GAS);
        this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, true);
        changed = true;
    }
    
    // Away state detection
    if (verify_state(data, get_state_away_on()))
    {
        if (!this->is_away())
        {
            this->set_state_flag_(water_heater::WATER_HEATER_STATE_AWAY, true);
            changed = true;
        }
    }
    else if (verify_state(data, get_state_away_off()))
    {
        if (this->is_away())
        {
            this->set_state_flag_(water_heater::WATER_HEATER_STATE_AWAY, false);
            changed = true;
        }
    }
    
    // On/Off state detection
    if (verify_state(data, get_state_on()))
    {
        if (!this->is_on())
        {
            this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, true);
            changed = true;
        }
    }
    
    // Current temperature
    if (this->sensor_ == nullptr)
    {
        optional<float> val = get_state_temperature_current(data);
        if (val.has_value() && this->get_current_temperature() != val.value())
        {
            this->set_current_temperature(val.value());
            changed = true;
        }
    }
    
    // Target temperature
    {
        optional<float> val = get_state_temperature_target(data);
        if (val.has_value() && this->get_target_temperature() != val.value())
        {
            this->set_target_temperature_(val.value());
            changed = true;
        }
    }
    
    if (changed) publish_state();
}

void UARTExWaterHeater::control(const water_heater::WaterHeaterCall& call)
{
    // Turn on first so that a following mode command reaches a powered device
    if (call.get_on().has_value() && *call.get_on())
    {
        if (!this->is_on() && enqueue_and_reflect(get_command_on()))
        {
            this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, true);
        }
    }
    
    // Set mode (OFF is still sent while the ON flag is set, even if the mode is already OFF)
    bool off_sent = false;
    if (call.get_mode().has_value())
    {
        water_heater::WaterHeaterMode mode = *call.get_mode();
        if (this->get_mode() != mode || (mode == water_heater::WATER_HEATER_MODE_OFF && this->is_on()))
        {
            const cmd_t* cmd = nullptr;
            if (mode == water_heater::WATER_HEATER_MODE_OFF)
            {
                cmd = get_command_off();
                off_sent = true;
            }
            else if (mode == water_heater::WATER_HEATER_MODE_ECO) cmd = get_command_eco();
            else if (mode == water_heater::WATER_HEATER_MODE_ELECTRIC) cmd = get_command_electric();
            else if (mode == water_heater::WATER_HEATER_MODE_PERFORMANCE) cmd = get_command_performance();
            else if (mode == water_heater::WATER_HEATER_MODE_HIGH_DEMAND) cmd = get_command_high_demand();
            else if (mode == water_heater::WATER_HEATER_MODE_HEAT_PUMP) cmd = get_command_heat_pump();
            else if (mode == water_heater::WATER_HEATER_MODE_GAS) cmd = get_command_gas();
            
            if (enqueue_and_reflect(cmd))
            {
                this->set_mode_(mode);
                this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, mode != water_heater::WATER_HEATER_MODE_OFF);
            }
        }
    }
    
    // Set target temperature
    if (!std::isnan(call.get_target_temperature()) && this->get_target_temperature() != call.get_target_temperature())
    {
        float temperature = call.get_target_temperature();
        if (enqueue_and_reflect(get_command_temperature(temperature)))
        {
            this->set_target_temperature_(temperature);
        }
    }
    
    // Set away mode (only when the call carries an away value)
    if (call.get_away().has_value())
    {
        const bool away = *call.get_away();
        if (away != this->is_away() && enqueue_and_reflect(away ? get_command_away_on() : get_command_away_off()))
        {
            this->set_state_flag_(water_heater::WATER_HEATER_STATE_AWAY, away);
        }
    }
    
    // Turn off last (skipped when the mode OFF command was already sent)
    if (call.get_on().has_value() && !*call.get_on() && !off_sent)
    {
        if ((this->is_on() || this->get_mode() != water_heater::WATER_HEATER_MODE_OFF) && enqueue_and_reflect(get_command_off()))
        {
            this->set_mode_(water_heater::WATER_HEATER_MODE_OFF);
            this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, false);
        }
    }
    
    publish_state();
}

}  // namespace uartex
}  // namespace esphome
