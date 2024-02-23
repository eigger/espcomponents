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
    traits.set_supports_current_temperature(true);
    for (auto mode : supported_mode_) traits.add_supported_mode(mode);
    for (auto mode : supported_swing_mode_) traits.add_supported_swing_mode(mode);
    for (auto preset : supported_preset_) traits.add_supported_preset(preset);
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
}

void UARTExClimate::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    //Mode
    if (this->state_off_.has_value() && verify_state(data, &this->state_off_.value()))
    {
        if (this->mode != climate::CLIMATE_MODE_OFF)
        {
            this->mode = climate::CLIMATE_MODE_OFF;
            changed = true;
        }
    }
    else if (this->state_cool_.has_value() && verify_state(data, &this->state_cool_.value()))
    {
        if (this->mode != climate::CLIMATE_MODE_COOL)
        {
            this->mode = climate::CLIMATE_MODE_COOL;
            changed = true;
        }
    }
    else if (this->state_heat_.has_value() && verify_state(data, &this->state_heat_.value()))
    {
        if (this->mode != climate::CLIMATE_MODE_HEAT)
        {
            this->mode = climate::CLIMATE_MODE_HEAT;
            changed = true;
        }
    }
    else if (this->state_fan_only_.has_value() && verify_state(data, &this->state_fan_only_.value()))
    {
        if (this->mode != climate::CLIMATE_MODE_FAN_ONLY)
        {
            this->mode = climate::CLIMATE_MODE_FAN_ONLY;
            changed = true;
        }
    }
    else if (this->state_dry_.has_value() && verify_state(data, &this->state_dry_.value()))
    {
        if (this->mode != climate::CLIMATE_MODE_DRY)
        {
            this->mode = climate::CLIMATE_MODE_DRY;
            changed = true;
        }
    }
    else if (this->state_auto_.has_value() && verify_state(data, &this->state_auto_.value()))
    {
        if (this->mode != climate::CLIMATE_MODE_AUTO)
        {
            this->mode = climate::CLIMATE_MODE_AUTO;
            changed = true;
        }
    }

    //Swing Mode
    if (this->state_swing_off_.has_value() && verify_state(data, &this->state_swing_off_.value()))
    {
        if (this->swing_mode != climate::CLIMATE_SWING_OFF)
        {
            this->swing_mode = climate::CLIMATE_SWING_OFF;
            changed = true;
        }
    }
    else if (this->state_swing_both_.has_value() && verify_state(data, &this->state_swing_both_.value()))
    {
        if (this->swing_mode != climate::CLIMATE_SWING_BOTH)
        {
            this->swing_mode = climate::CLIMATE_SWING_BOTH;
            changed = true;
        }
    }
    else if (this->state_swing_vertical_.has_value() && verify_state(data, &this->state_swing_vertical_.value()))
    {
        if (this->swing_mode != climate::CLIMATE_SWING_VERTICAL)
        {
            this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
            changed = true;
        }
    }
    else if (this->state_swing_horizontal_.has_value() && verify_state(data, &this->state_swing_horizontal_.value()))
    {
        if (this->swing_mode != climate::CLIMATE_SWING_HORIZONTAL)
        {
            this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
            changed = true;
        }
    }

    // Current temperature
    if (this->sensor_ == nullptr)
    {
        if (this->state_current_func_.has_value())
        {
            optional<float> val = (*this->state_current_func_)(&data[0], data.size());
            if (val.has_value() && this->current_temperature != val.value())
            {
                this->current_temperature = val.value();
                changed = true;
            }
        }
        else if (this->state_current_.has_value() && data.size() >= (this->state_current_.value().offset + this->state_current_.value().length))
        {
            float val = state_to_float(data, this->state_current_.value());
            if (this->current_temperature != val)
            {
                this->current_temperature = val;
                changed = true;
            }
        }
    }

    // Target temperature
    if (this->state_target_func_.has_value())
    {
        optional<float> val = (*this->state_target_func_)(&data[0], data.size());
        if (val.has_value() && this->target_temperature != val.value())
        {
            this->target_temperature = val.value();
            changed = true;
        }
    }
    else if (this->state_target_.has_value() && data.size() >= (this->state_target_.value().offset + this->state_target_.value().length))
    {
        float val = state_to_float(data, this->state_target_.value());
        if (this->target_temperature != val)
        {
            this->target_temperature = val;
            changed = true;
        }
    }

    if (changed) publish_state();
}

void UARTExClimate::control(const climate::ClimateCall &call)
{
    // Set mode
    if (call.get_mode().has_value() && this->mode != *call.get_mode())
    {
        this->mode = *call.get_mode();
        if (this->mode == climate::CLIMATE_MODE_OFF)
        {
            enqueue_tx_cmd(get_command_off());
        }
        else if (this->mode == climate::CLIMATE_MODE_HEAT && this->command_heat_.has_value())
        {
            enqueue_tx_cmd(&this->command_heat_.value());
        }
        else if (this->mode == climate::CLIMATE_MODE_COOL && this->command_cool_.has_value())
        {
            enqueue_tx_cmd(&this->command_cool_.value());
        }
        else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY && this->command_fan_only_.has_value())
        {
            enqueue_tx_cmd(&this->command_fan_only_.value());
        }
        else if (this->mode == climate::CLIMATE_MODE_DRY && this->command_dry_.has_value())
        {
            enqueue_tx_cmd(&this->command_dry_.value());
        }
        else if (this->mode == climate::CLIMATE_MODE_AUTO && this->command_auto_.has_value())
        {
            enqueue_tx_cmd(&this->command_auto_.value());
        }
    }

    // Set target temperature
    if (call.get_target_temperature().has_value() && this->target_temperature != *call.get_target_temperature())
    {
        this->target_temperature = *call.get_target_temperature();
        this->command_temperature_ = (this->command_temperature_func_)(this->target_temperature, this->mode, *this->preset);
        enqueue_tx_cmd(&this->command_temperature_);
    }

    // Set swing mode
    if (call.get_swing_mode().has_value() && this->swing_mode != *call.get_swing_mode())
    {
        this->swing_mode = *call.get_swing_mode();
        if (this->swing_mode == climate::CLIMATE_SWING_OFF && this->command_swing_off_.has_value())
        {
            enqueue_tx_cmd(&this->command_swing_off_.value());
        }
        else if (this->swing_mode == climate::CLIMATE_SWING_BOTH && this->command_swing_both_.has_value())
        {
            enqueue_tx_cmd(&this->command_swing_both_.value());
        }
        else if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL && this->command_swing_vertical_.has_value())
        {
            enqueue_tx_cmd(&this->command_swing_vertical_.value());
        }
        else if (this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL && this->command_swing_horizontal_.has_value())
        {
            enqueue_tx_cmd(&this->command_swing_horizontal_.value());
        }
    }

    // Set preset
    if (call.get_preset().has_value() && this->preset != *call.get_preset())
    {
        this->preset = *call.get_preset();
        if (this->preset == climate::CLIMATE_PRESET_NONE && this->command_preset_none_.has_value())
        {
            enqueue_tx_cmd(&this->command_preset_none_.value());
        }
        else if (this->preset == climate::CLIMATE_PRESET_HOME && this->command_preset_home_.has_value())
        {
            enqueue_tx_cmd(&this->command_preset_home_.value());
        }
        else if (this->preset == climate::CLIMATE_PRESET_AWAY && this->command_preset_away_.has_value())
        {
            enqueue_tx_cmd(&this->command_preset_away_.value());
        }
        else if (this->preset == climate::CLIMATE_PRESET_BOOST && this->command_preset_boost_.has_value())
        {
            enqueue_tx_cmd(&this->command_preset_boost_.value());
        }
        else if (this->preset == climate::CLIMATE_PRESET_COMFORT && this->command_preset_comfort_.has_value())
        {
            enqueue_tx_cmd(&this->command_preset_comfort_.value());
        }
        else if (this->preset == climate::CLIMATE_PRESET_ECO && this->command_preset_eco_.has_value())
        {
            enqueue_tx_cmd(&this->command_preset_eco_.value());
        }
        else if (this->preset == climate::CLIMATE_PRESET_SLEEP && this->command_preset_sleep_.has_value())
        {
            enqueue_tx_cmd(&this->command_preset_sleep_.value());
        }
        else if (this->preset == climate::CLIMATE_PRESET_ACTIVITY && this->command_preset_activity_.has_value())
        {
            enqueue_tx_cmd(&this->command_preset_activity_.value());
        }
    }
    publish_state();
}

}  // namespace uartex
}  // namespace esphome
