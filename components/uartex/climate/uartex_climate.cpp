#include "uartex_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.climate";

void UARTExClimate::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Climate '%s':", get_name().c_str());
    dump_uartex_device_config(TAG);
}

climate::ClimateTraits UARTExClimate::traits()
{
    auto traits = climate::ClimateTraits();
    if (this->sensor_ != nullptr || this->state_current_temperature_func_.has_value() || this->state_current_temperature_.has_value())
    {
        traits.set_supports_current_temperature(true);
    }
    if (this->state_current_humidity_func_.has_value() || this->state_current_humidity_.has_value())
    {
        traits.set_supports_current_humidity(true);
    }
    if (this->state_target_humidity_func_.has_value() || this->state_target_humidity_.has_value() || this->command_humidity_func_ != nullptr)
    {
        traits.set_supports_target_humidity(true);
    }

    for (uint8_t mode = climate::CLIMATE_MODE_COOL; mode <= climate::CLIMATE_MODE_AUTO; mode++)
    {
        bool add = false;
        if (command_mode_.find((climate::ClimateMode)mode) != command_mode_.end()) add = true;
        else if (state_mode_.find((climate::ClimateMode)mode) != state_mode_.end()) add = true;
        else if (command_mode_func_.find((climate::ClimateMode)mode) != command_mode_func_.end()) add = true;
        if (add) traits.add_supported_mode((climate::ClimateMode)mode);
    }
    for (uint8_t mode = climate::CLIMATE_SWING_OFF; mode <= climate::CLIMATE_SWING_HORIZONTAL; mode++)
    {
        bool add = false;
        if (command_swing_mode_.find((climate::ClimateSwingMode)mode) != command_swing_mode_.end()) add = true;
        else if (state_swing_mode_.find((climate::ClimateSwingMode)mode) != state_swing_mode_.end()) add = true;
        else if (command_swing_mode_func_.find((climate::ClimateSwingMode)mode) != command_swing_mode_func_.end()) add = true;
        if (add) traits.add_supported_swing_mode((climate::ClimateSwingMode)mode);
    }
    for (uint8_t mode = climate::CLIMATE_FAN_ON; mode <= climate::CLIMATE_FAN_QUIET; mode++)
    {
        bool add = false;
        if (command_fan_mode_.find((climate::ClimateFanMode)mode) != command_fan_mode_.end()) add = true;
        else if (state_fan_mode_.find((climate::ClimateFanMode)mode) != state_fan_mode_.end()) add = true;
        else if (command_fan_mode_func_.find((climate::ClimateFanMode)mode) != command_fan_mode_func_.end()) add = true;
        if (add) traits.add_supported_fan_mode((climate::ClimateFanMode)mode);
    }
    for (uint8_t preset = climate::CLIMATE_PRESET_NONE; preset <= climate::CLIMATE_PRESET_ACTIVITY; preset++)
    {
        bool add = false;
        if (command_preset_.find((climate::ClimatePreset)preset) != command_preset_.end()) add = true;
        else if (state_preset_.find((climate::ClimatePreset)preset) != state_preset_.end()) add = true;
        else if (command_preset_func_.find((climate::ClimatePreset)preset) != command_preset_func_.end()) add = true;
        if (add) traits.add_supported_preset((climate::ClimatePreset)preset);
    }
    traits.set_supports_two_point_target_temperature(false);
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
        changed = true;
    }
    else if (verify_state(data, get_state_cool()))
    {
        this->mode = climate::CLIMATE_MODE_COOL;
        changed = true;
    }
    else if (verify_state(data, get_state_heat()))
    {
        this->mode = climate::CLIMATE_MODE_HEAT;
        changed = true;
    }
    else if (verify_state(data, get_state_fan_only()))
    {
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        changed = true;
    }
    else if (verify_state(data, get_state_dry()))
    {
        this->mode = climate::CLIMATE_MODE_DRY;
        changed = true;
    }
    else if (verify_state(data, get_state_auto()))
    {
        this->mode = climate::CLIMATE_MODE_AUTO;
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
    
    // Current temperature
    if (this->sensor_ == nullptr)
    {
        optional<float> val = get_state_num("state_current_temperature", data);
        if (val.has_value() && this->current_temperature != val.value())
        {
            this->current_temperature = val.value();
            changed = true;
        }
    }

    // Target temperature
    optional<float> val = get_state_num("state_target_temperature", data);
    if (val.has_value() && this->target_temperature != val.value())
    {
        this->target_temperature = val.value();
        changed = true;
    }

    // Current humidity
    optional<float> val = get_state_num("state_current_humidity", data);
    if (val.has_value() && this->current_humidity != val.value())
    {
        this->current_humidity = val.value();
        changed = true;
    }

    // Target humidity
    optional<float> val = get_state_num("state_target_humidity", data);
    if (val.has_value() && this->target_humidity != val.value())
    {
        this->target_humidity = val.value();
        changed = true;
    }

    if (changed) publish_state();
}

void UARTExClimate::control(const climate::ClimateCall &call)
{
    // Set mode
    if (call.get_mode().has_value() && this->mode != *call.get_mode())
    {
        this->mode = *call.get_mode();
        if (this->mode == climate::CLIMATE_MODE_OFF) enqueue_tx_cmd(get_command_off());
        else if (this->mode == climate::CLIMATE_MODE_COOL) enqueue_tx_cmd(get_command_cool());
        else if (this->mode == climate::CLIMATE_MODE_HEAT) enqueue_tx_cmd(get_command_heat());
        else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) enqueue_tx_cmd(get_command_fan_only());
        else if (this->mode == climate::CLIMATE_MODE_DRY) enqueue_tx_cmd(get_command_dry());
        else if (this->mode == climate::CLIMATE_MODE_AUTO) enqueue_tx_cmd(get_command_auto());
    }

    // Set target temperature
    if (call.get_target_temperature().has_value() && this->target_temperature != *call.get_target_temperature())
    {
        this->target_temperature = *call.get_target_temperature();
        enqueue_tx_cmd(get_command_temperature(this->target_temperature));
    }

    // Set target humidity
    if (call.get_target_humidity().has_value() && this->target_humidity != *call.get_target_humidity())
    {
        this->target_humidity = *call.get_target_humidity();
        enqueue_tx_cmd(get_command_humidity(this->target_humidity));
    }

    // Set swing mode
    if (call.get_swing_mode().has_value() && this->swing_mode != *call.get_swing_mode())
    {
        this->swing_mode = *call.get_swing_mode();
        if (this->swing_mode == climate::CLIMATE_SWING_OFF) enqueue_tx_cmd(get_command_swing_off());
        else if (this->swing_mode == climate::CLIMATE_SWING_BOTH) enqueue_tx_cmd(get_command_swing_both());
        else if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) enqueue_tx_cmd(get_command_swing_vertical());
        else if (this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL) enqueue_tx_cmd(get_command_swing_horizontal());
    }

    // Set fan mode
    if (call.get_fan_mode().has_value() && this->fan_mode != *call.get_fan_mode())
    {
        this->fan_mode = *call.get_fan_mode();
        if (this->fan_mode.value() == climate::CLIMATE_FAN_ON) enqueue_tx_cmd(get_command_fan_on());
        else if (this->fan_mode.value() == climate::CLIMATE_FAN_OFF) enqueue_tx_cmd(get_command_fan_off());
        else if (this->fan_mode.value() == climate::CLIMATE_FAN_AUTO) enqueue_tx_cmd(get_command_fan_auto());
        else if (this->fan_mode.value() == climate::CLIMATE_FAN_LOW) enqueue_tx_cmd(get_command_fan_low());
        else if (this->fan_mode.value() == climate::CLIMATE_FAN_MEDIUM) enqueue_tx_cmd(get_command_fan_medium());
        else if (this->fan_mode.value() == climate::CLIMATE_FAN_HIGH) enqueue_tx_cmd(get_command_fan_high());
        else if (this->fan_mode.value() == climate::CLIMATE_FAN_MIDDLE) enqueue_tx_cmd(get_command_fan_middle());
        else if (this->fan_mode.value() == climate::CLIMATE_FAN_FOCUS) enqueue_tx_cmd(get_command_fan_focus());
        else if (this->fan_mode.value() == climate::CLIMATE_FAN_DIFFUSE) enqueue_tx_cmd(get_command_fan_diffuse());
        else if (this->fan_mode.value() == climate::CLIMATE_FAN_QUIET) enqueue_tx_cmd(get_command_fan_quiet());
    }

    // Set preset
    if (call.get_preset().has_value() && this->preset != *call.get_preset())
    {
        this->preset = *call.get_preset();
        if (this->preset.value() == climate::CLIMATE_PRESET_NONE) enqueue_tx_cmd(get_command_preset_none());
        else if (this->preset.value() == climate::CLIMATE_PRESET_HOME) enqueue_tx_cmd(get_command_preset_home());
        else if (this->preset.value() == climate::CLIMATE_PRESET_AWAY) enqueue_tx_cmd(get_command_preset_away());
        else if (this->preset.value() == climate::CLIMATE_PRESET_BOOST) enqueue_tx_cmd(get_command_preset_boost());
        else if (this->preset.value() == climate::CLIMATE_PRESET_COMFORT) enqueue_tx_cmd(get_command_preset_comfort());
        else if (this->preset.value() == climate::CLIMATE_PRESET_ECO) enqueue_tx_cmd(get_command_preset_eco());
        else if (this->preset.value() == climate::CLIMATE_PRESET_SLEEP) enqueue_tx_cmd(get_command_preset_sleep());
        else if (this->preset.value() == climate::CLIMATE_PRESET_ACTIVITY) enqueue_tx_cmd(get_command_preset_activity());
    }
    publish_state();
}

}  // namespace uartex
}  // namespace esphome
