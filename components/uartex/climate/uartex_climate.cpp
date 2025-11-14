#include "uartex_climate.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.climate";

void UARTExClimate::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    log_config(TAG, "State Temperature Current", get_state_num("state_temperature_current"));
    log_config(TAG, "State Temperature Target", get_state_num("state_temperature_target"));
    log_config(TAG, "State Humidity Current", get_state_num("state_humidity_current"));
    log_config(TAG, "State Humidity Target", get_state_num("state_humidity_target"));
    log_config(TAG, "State Cool", get_state_cool());
    log_config(TAG, "State Heat", get_state_heat());
    log_config(TAG, "State Fan Only", get_state_fan_only());
    log_config(TAG, "State Dry", get_state_dry());
    log_config(TAG, "State Auto", get_state_auto());
    log_config(TAG, "State Fan On", get_state_fan_on());
    log_config(TAG, "State Fan Off", get_state_fan_off());
    log_config(TAG, "State Fan Auto", get_state_fan_auto());
    log_config(TAG, "State Fan Low", get_state_fan_low());
    log_config(TAG, "State Fan Medium", get_state_fan_medium());
    log_config(TAG, "State Fan High", get_state_fan_high());
    log_config(TAG, "State Fan Middle", get_state_fan_middle());
    log_config(TAG, "State Fan Focus", get_state_fan_focus());
    log_config(TAG, "State Fan Diffuse", get_state_fan_diffuse());
    log_config(TAG, "State Fan Quiet", get_state_fan_quiet());
    log_config(TAG, "State Swing Off", get_state_swing_off());
    log_config(TAG, "State Swing Both", get_state_swing_both());
    log_config(TAG, "State Swing Vertical", get_state_swing_vertical());
    log_config(TAG, "State Swing Horizontal", get_state_swing_horizontal());
    log_config(TAG, "State Preset None", get_state_preset_none());
    log_config(TAG, "State Preset Home", get_state_preset_home());
    log_config(TAG, "State Preset Away", get_state_preset_away());
    log_config(TAG, "State Preset Boost", get_state_preset_boost());
    log_config(TAG, "State Preset Comfort", get_state_preset_comfort());
    log_config(TAG, "State Preset Eco", get_state_preset_eco());
    log_config(TAG, "State Preset Sleep", get_state_preset_sleep());
    log_config(TAG, "State Preset Activity", get_state_preset_activity());
    log_config(TAG, "Command Cool", get_command_cool());
    log_config(TAG, "Command Heat", get_command_heat());
    log_config(TAG, "Command Fan Only", get_command_fan_only());
    log_config(TAG, "Command Dry", get_command_dry());
    log_config(TAG, "Command Auto", get_command_auto());
    log_config(TAG, "Command Fan On", get_command_fan_on());
    log_config(TAG, "Command Fan Off", get_command_fan_off());
    log_config(TAG, "Command Fan Auto", get_command_fan_auto());
    log_config(TAG, "Command Fan Low", get_command_fan_low());
    log_config(TAG, "Command Fan Medium", get_command_fan_medium());
    log_config(TAG, "Command Fan High", get_command_fan_high());
    log_config(TAG, "Command Fan Middle", get_command_fan_middle());
    log_config(TAG, "Command Fan Focus", get_command_fan_focus());
    log_config(TAG, "Command Fan Diffuse", get_command_fan_diffuse());
    log_config(TAG, "Command Fan Quiet", get_command_fan_quiet());
    log_config(TAG, "Command Swing Off", get_command_swing_off());
    log_config(TAG, "Command Swing Both", get_command_swing_both());
    log_config(TAG, "Command Swing Vertical", get_command_swing_vertical());
    log_config(TAG, "Command Swing Horizontal", get_command_swing_horizontal());
    log_config(TAG, "Command Preset None", get_command_preset_none());
    log_config(TAG, "Command Preset Home", get_command_preset_home());
    log_config(TAG, "Command Preset Away", get_command_preset_away());
    log_config(TAG, "Command Preset Boost", get_command_preset_boost());
    log_config(TAG, "Command Preset Comfort", get_command_preset_comfort());
    log_config(TAG, "Command Preset Eco", get_command_preset_eco());
    log_config(TAG, "Command Preset Sleep", get_command_preset_sleep());
    log_config(TAG, "Command Preset Activity", get_command_preset_activity());
    uartex_dump_config(TAG);
#endif
}

climate::ClimateTraits UARTExClimate::traits()
{
    auto traits = climate::ClimateTraits();
    if (this->sensor_ != nullptr || has_state_temperature_current())
    {
        traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    }
    if (has_state_humidity_current())
    {
        traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY);
    }
    if (has_state_humidity_target() || get_command_humidity(0))
    {
        traits.add_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY);
    }
    if (!this->custom_fan_modes_.empty()) traits.set_supported_custom_fan_modes(this->custom_fan_modes_);
    if (!this->custom_preset_modes_.empty()) traits.set_supported_custom_presets(this->custom_preset_modes_);

    if (get_command_cool() || get_state_cool()) traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
    if (get_command_heat() || get_state_heat()) traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
    if (get_command_fan_only() || get_state_fan_only()) traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);
    if (get_command_dry() || get_state_dry()) traits.add_supported_mode(climate::CLIMATE_MODE_DRY);
    if (get_command_auto() || get_state_auto()) traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);

    if (get_command_swing_off() || get_state_swing_off()) traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
    if (get_command_swing_both() || get_state_swing_both()) traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
    if (get_command_swing_vertical() || get_state_swing_vertical()) traits.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    if (get_command_swing_horizontal() || get_state_swing_horizontal()) traits.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

    if (get_command_fan_on() || get_state_fan_on()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_ON);
    if (get_command_fan_off() || get_state_fan_off()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_OFF);
    if (get_command_fan_auto() || get_state_fan_auto()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
    if (get_command_fan_low() || get_state_fan_low()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_LOW);
    if (get_command_fan_medium() || get_state_fan_medium()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_MEDIUM);
    if (get_command_fan_high() || get_state_fan_high()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_HIGH);
    if (get_command_fan_middle() || get_state_fan_middle()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_MIDDLE);
    if (get_command_fan_focus() || get_state_fan_focus()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_FOCUS);
    if (get_command_fan_diffuse() || get_state_fan_diffuse()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_DIFFUSE);
    if (get_command_fan_quiet() || get_state_fan_quiet()) traits.add_supported_fan_mode(climate::CLIMATE_FAN_QUIET);

    if (get_command_preset_none() || get_state_preset_none()) traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
    if (get_command_preset_home() || get_state_preset_home()) traits.add_supported_preset(climate::CLIMATE_PRESET_HOME);
    if (get_command_preset_away() || get_state_preset_away()) traits.add_supported_preset(climate::CLIMATE_PRESET_AWAY);
    if (get_command_preset_boost() || get_state_preset_boost()) traits.add_supported_preset(climate::CLIMATE_PRESET_BOOST);
    if (get_command_preset_comfort() || get_state_preset_comfort()) traits.add_supported_preset(climate::CLIMATE_PRESET_COMFORT);
    if (get_command_preset_eco() || get_state_preset_eco()) traits.add_supported_preset(climate::CLIMATE_PRESET_ECO);
    if (get_command_preset_sleep() || get_state_preset_sleep()) traits.add_supported_preset(climate::CLIMATE_PRESET_SLEEP);
    if (get_command_preset_activity() || get_state_preset_activity()) traits.add_supported_preset(climate::CLIMATE_PRESET_ACTIVITY);

    if (get_state_action_cooling() || get_state_action_heating() || get_state_action_idle() || get_state_action_drying() || get_state_action_fan())
    {
        traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
    }
    traits.clear_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
    return traits;
}

void UARTExClimate::setup()
{
    this->target_temperature = NAN;
    if (this->sensor_)
    {
        this->sensor_->add_on_state_callback([this](float state)
                                                {
                                                    this->current_temperature = state;
                                                    publish_state();
                                                });
        this->current_temperature = this->sensor_->state;
    }
    else this->current_temperature = NAN;
    this->target_humidity = NAN;
    this->current_humidity = NAN;
}

void UARTExClimate::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    //Mode
    if (verify_state(data, get_state_off()))
    {
        this->mode = climate::CLIMATE_MODE_OFF;
        this->action = mode_to_action(this->mode);
        changed = true;
    }
    else if (verify_state(data, get_state_cool()))
    {
        this->mode = climate::CLIMATE_MODE_COOL;
        this->action = mode_to_action(this->mode);
        changed = true;
    }
    else if (verify_state(data, get_state_heat()))
    {
        this->mode = climate::CLIMATE_MODE_HEAT;
        this->action = mode_to_action(this->mode);
        changed = true;
    }
    else if (verify_state(data, get_state_fan_only()))
    {
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        this->action = mode_to_action(this->mode);
        changed = true;
    }
    else if (verify_state(data, get_state_dry()))
    {
        this->mode = climate::CLIMATE_MODE_DRY;
        this->action = mode_to_action(this->mode);
        changed = true;
    }
    else if (verify_state(data, get_state_auto()))
    {
        this->mode = climate::CLIMATE_MODE_AUTO;
        changed = true;
    }

    //Action
    if (verify_state(data, get_state_action_cooling()))
    {
        this->action = climate::CLIMATE_ACTION_COOLING;
        changed = true;
    }
    else if (verify_state(data, get_state_action_heating()))
    {
        this->action = climate::CLIMATE_ACTION_HEATING;
        changed = true;
    }
    else if (verify_state(data, get_state_action_idle()))
    {
        this->action = climate::CLIMATE_ACTION_IDLE;
        changed = true;
    }
    else if (verify_state(data, get_state_action_drying()))
    {
        this->action = climate::CLIMATE_ACTION_DRYING;
        changed = true;
    }
    else if (verify_state(data, get_state_action_fan()))
    {
        this->action = climate::CLIMATE_ACTION_FAN;
        changed = true;
    }

    //Swing Mode
    if (verify_state(data, get_state_swing_off()))
    {
        this->swing_mode = climate::CLIMATE_SWING_OFF;
        changed = true;
    }
    else if (verify_state(data, get_state_swing_both()))
    {
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
        changed = true;
    }
    else if (verify_state(data, get_state_swing_vertical()))
    {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        changed = true;
    }
    else if (verify_state(data, get_state_swing_horizontal()))
    {
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
        changed = true;
    }

    //Fan Mode
    if (verify_state(data, get_state_fan_on()))
    {
        this->fan_mode = climate::CLIMATE_FAN_ON;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_off()))
    {
        this->fan_mode = climate::CLIMATE_FAN_OFF;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_auto()))
    {
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_low()))
    {
        this->fan_mode = climate::CLIMATE_FAN_LOW;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_medium()))
    {
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_high()))
    {
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_middle()))
    {
        this->fan_mode = climate::CLIMATE_FAN_MIDDLE;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_focus()))
    {
        this->fan_mode = climate::CLIMATE_FAN_FOCUS;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_diffuse()))
    {
        this->fan_mode = climate::CLIMATE_FAN_DIFFUSE;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_on()))
    {
        this->fan_mode = climate::CLIMATE_FAN_QUIET;
        changed = true;
    }

    //Preset
    if (verify_state(data, get_state_preset_none()))
    {
        this->preset = climate::CLIMATE_PRESET_NONE;
        changed = true;
    }
    else if (verify_state(data, get_state_preset_home()))
    {
        this->preset = climate::CLIMATE_PRESET_HOME;
        changed = true;
    }
    else if (verify_state(data, get_state_preset_away()))
    {
        this->preset = climate::CLIMATE_PRESET_AWAY;
        changed = true;
    }
    else if (verify_state(data, get_state_preset_boost()))
    {
        this->preset = climate::CLIMATE_PRESET_BOOST;
        changed = true;
    }
    else if (verify_state(data, get_state_preset_comfort()))
    {
        this->preset = climate::CLIMATE_PRESET_COMFORT;
        changed = true;
    }
    else if (verify_state(data, get_state_preset_eco()))
    {
        this->preset = climate::CLIMATE_PRESET_ECO;
        changed = true;
    }
    else if (verify_state(data, get_state_preset_sleep()))
    {
        this->preset = climate::CLIMATE_PRESET_SLEEP;
        changed = true;
    }
    else if (verify_state(data, get_state_preset_activity()))
    {
        this->preset = climate::CLIMATE_PRESET_ACTIVITY;
        changed = true;
    }

    // custom fan
    optional<std::string> custom_fan = get_state_custom_fan(data);
    if (custom_fan.has_value() && (this->get_custom_fan_mode() == nullptr || this->get_custom_fan_mode() != custom_fan.value()))
    {
        const char* fan_char = find_mode(custom_fan_modes_, custom_fan.value());
        if (fan_char != nullptr && this->set_custom_fan_mode_(fan_char)) changed = true;
    }

    // custom preset
    optional<std::string> custom_preset = get_state_custom_preset(data);
    if (custom_preset.has_value() && (this->get_custom_preset() == nullptr || this->get_custom_preset() != custom_preset.value()))
    {
        const char* preset_char = find_mode(custom_preset_modes_, custom_preset.value());
        if (preset_char != nullptr && this->set_custom_preset_(preset_char)) changed = true;
    }
    
    // Current temperature
    if (this->sensor_ == nullptr)
    {
        optional<float> val = get_state_temperature_current(data);
        if (val.has_value() && this->current_temperature != val.value())
        {
            this->current_temperature = val.value();
            changed = true;
        }
    }

    // Target temperature
    {
        optional<float> val = get_state_temperature_target(data);
        if (val.has_value() && this->target_temperature != val.value())
        {
            this->target_temperature = val.value();
            changed = true;
        }
    }


    // Current humidity
    {
        optional<float> val = get_state_humidity_current(data);
        if (val.has_value() && this->current_humidity != val.value())
        {
            this->current_humidity = val.value();
            changed = true;
        }
    }


    // Target humidity
    {
        optional<float> val = get_state_humidity_target(data);
        if (val.has_value() && this->target_humidity != val.value())
        {
            this->target_humidity = val.value();
            changed = true;
        }
    }


    if (changed) publish_state();
}

climate::ClimateAction UARTExClimate::mode_to_action(climate::ClimateMode mode)
{
    climate::ClimateAction action = this->action;
    if (mode == climate::CLIMATE_MODE_OFF) action = climate::CLIMATE_ACTION_OFF;
    else if (mode == climate::CLIMATE_MODE_COOL) action = climate::CLIMATE_ACTION_COOLING;
    else if (mode == climate::CLIMATE_MODE_HEAT) action = climate::CLIMATE_ACTION_HEATING;
    else if (mode == climate::CLIMATE_MODE_FAN_ONLY) action = climate::CLIMATE_ACTION_FAN;
    else if (mode == climate::CLIMATE_MODE_DRY) action = climate::CLIMATE_ACTION_DRYING;
    return action;
}

void UARTExClimate::control(const climate::ClimateCall& call)
{
    bool changed = false;
    // Set mode
    if (call.get_mode().has_value() && this->mode != *call.get_mode())
    {
        climate::ClimateMode mode = *call.get_mode();
        if (mode == climate::CLIMATE_MODE_OFF) changed = enqueue_tx_cmd(get_command_off());
        else if (mode == climate::CLIMATE_MODE_COOL) changed = enqueue_tx_cmd(get_command_cool());
        else if (mode == climate::CLIMATE_MODE_HEAT) changed = enqueue_tx_cmd(get_command_heat());
        else if (mode == climate::CLIMATE_MODE_FAN_ONLY) changed = enqueue_tx_cmd(get_command_fan_only());
        else if (mode == climate::CLIMATE_MODE_DRY) changed = enqueue_tx_cmd(get_command_dry());
        else if (mode == climate::CLIMATE_MODE_AUTO) changed = enqueue_tx_cmd(get_command_auto());
        if (changed || this->optimistic_) 
        {
            this->mode = mode;
            this->action = mode_to_action(mode);
        }
    }

    // Set target temperature
    if (call.get_target_temperature().has_value() && this->target_temperature != *call.get_target_temperature())
    {
        float temperature = *call.get_target_temperature();
        if (enqueue_tx_cmd(get_command_temperature(temperature)) || this->optimistic_)
        {
            this->target_temperature = temperature;
        }
    }

    // Set target humidity
    if (call.get_target_humidity().has_value() && this->target_humidity != *call.get_target_humidity())
    {
        float humidity = *call.get_target_humidity();
        if (enqueue_tx_cmd(get_command_humidity(humidity)) || this->optimistic_)
        {
            this->target_humidity = humidity;
        }
    }

    // Set swing mode
    if (call.get_swing_mode().has_value() && this->swing_mode != *call.get_swing_mode())
    {
        changed = false;
        climate::ClimateSwingMode swing_mode = *call.get_swing_mode();
        if (swing_mode == climate::CLIMATE_SWING_OFF) changed = enqueue_tx_cmd(get_command_swing_off());
        else if (swing_mode == climate::CLIMATE_SWING_BOTH) changed = enqueue_tx_cmd(get_command_swing_both());
        else if (swing_mode == climate::CLIMATE_SWING_VERTICAL) changed = enqueue_tx_cmd(get_command_swing_vertical());
        else if (swing_mode == climate::CLIMATE_SWING_HORIZONTAL) changed = enqueue_tx_cmd(get_command_swing_horizontal());
        if (changed || this->optimistic_) this->swing_mode = swing_mode;
    }

    // Set fan mode
    if (call.get_fan_mode().has_value() && this->fan_mode != *call.get_fan_mode())
    {
        changed = false;
        optional<climate::ClimateFanMode> fan_mode = *call.get_fan_mode();
        if (fan_mode.value() == climate::CLIMATE_FAN_ON) changed = enqueue_tx_cmd(get_command_fan_on());
        else if (fan_mode.value() == climate::CLIMATE_FAN_OFF) changed = enqueue_tx_cmd(get_command_fan_off());
        else if (fan_mode.value() == climate::CLIMATE_FAN_AUTO) changed = enqueue_tx_cmd(get_command_fan_auto());
        else if (fan_mode.value() == climate::CLIMATE_FAN_LOW) changed = enqueue_tx_cmd(get_command_fan_low());
        else if (fan_mode.value() == climate::CLIMATE_FAN_MEDIUM) changed = enqueue_tx_cmd(get_command_fan_medium());
        else if (fan_mode.value() == climate::CLIMATE_FAN_HIGH) changed = enqueue_tx_cmd(get_command_fan_high());
        else if (fan_mode.value() == climate::CLIMATE_FAN_MIDDLE) changed = enqueue_tx_cmd(get_command_fan_middle());
        else if (fan_mode.value() == climate::CLIMATE_FAN_FOCUS) changed = enqueue_tx_cmd(get_command_fan_focus());
        else if (fan_mode.value() == climate::CLIMATE_FAN_DIFFUSE) changed = enqueue_tx_cmd(get_command_fan_diffuse());
        else if (fan_mode.value() == climate::CLIMATE_FAN_QUIET) changed = enqueue_tx_cmd(get_command_fan_quiet());
        if (changed || this->optimistic_) this->fan_mode = fan_mode;
    }

    // Set preset
    if (call.get_preset().has_value() && this->preset != *call.get_preset())
    {
        changed = false;
        optional<climate::ClimatePreset> preset = *call.get_preset();
        if (preset.value() == climate::CLIMATE_PRESET_NONE) changed = enqueue_tx_cmd(get_command_preset_none());
        else if (preset.value() == climate::CLIMATE_PRESET_HOME) changed = enqueue_tx_cmd(get_command_preset_home());
        else if (preset.value() == climate::CLIMATE_PRESET_AWAY) changed = enqueue_tx_cmd(get_command_preset_away());
        else if (preset.value() == climate::CLIMATE_PRESET_BOOST) changed = enqueue_tx_cmd(get_command_preset_boost());
        else if (preset.value() == climate::CLIMATE_PRESET_COMFORT) changed = enqueue_tx_cmd(get_command_preset_comfort());
        else if (preset.value() == climate::CLIMATE_PRESET_ECO) changed = enqueue_tx_cmd(get_command_preset_eco());
        else if (preset.value() == climate::CLIMATE_PRESET_SLEEP) changed = enqueue_tx_cmd(get_command_preset_sleep());
        else if (preset.value() == climate::CLIMATE_PRESET_ACTIVITY) changed = enqueue_tx_cmd(get_command_preset_activity());
        if (changed || this->optimistic_) this->preset = preset;
    }

    // custom fan
    if (call.has_custom_fan_mode() && (this->get_custom_fan_mode() == nullptr || std::strcmp(this->get_custom_fan_mode(), call.get_custom_fan_mode()) != 0))
    {
        const char* custom_fan_mode = call.get_custom_fan_mode();
        if (enqueue_tx_cmd(get_command_custom_fan(custom_fan_mode == nullptr ? "" : std::string(custom_fan_mode))) || this->optimistic_)
        {
            this->set_custom_fan_mode_(custom_fan_mode);
        }
    }

    // custom preset
    if (call.has_custom_preset() && (this->get_custom_preset() == nullptr || std::strcmp(this->get_custom_preset(), call.get_custom_preset()) != 0))
    {
        const char* custom_preset = call.get_custom_preset();
        if (enqueue_tx_cmd(get_command_custom_preset(custom_preset == nullptr ? "" : std::string(custom_preset))) || this->optimistic_)
        {
            this->set_custom_preset_(custom_preset);
        }
    }

    publish_state();
}

}  // namespace uartex
}  // namespace esphome
