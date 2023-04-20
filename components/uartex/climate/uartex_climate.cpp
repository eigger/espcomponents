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
    if (this->supports_auto_) traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);
    if (this->supports_cool_) traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
    if (this->supports_heat_) traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
    traits.set_supports_two_point_target_temperature(false);
    if (this->supports_away_)
    {
        traits.add_supported_preset(climate::CLIMATE_PRESET_AWAY);
        traits.add_supported_preset(climate::CLIMATE_PRESET_HOME);
    }
    traits.set_visual_min_temperature(5);
    traits.set_visual_max_temperature(40);
    traits.set_visual_temperature_step(1);
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
                                                    // current temperature changed, publish state
                                                    this->publish_state();
                                                });
        this->current_temperature = this->sensor_->state;
    }
    else this->current_temperature = NAN;
}

void UARTExClimate::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    // turn off
    if (this->state_off_.has_value() && validate(data, &state_off_.value()))
    {
        if (mode != climate::CLIMATE_MODE_OFF)
        {
            mode = climate::CLIMATE_MODE_OFF;
            changed = true;
        }
    }
    // heat mode
    else if (this->state_heat_.has_value() && validate(data, &state_heat_.value()))
    {
        if (mode != climate::CLIMATE_MODE_HEAT)
        {
            mode = climate::CLIMATE_MODE_HEAT;
            changed = true;
        }
    }
    // cool mode
    else if (this->state_cool_.has_value() && validate(data, &state_cool_.value()))
    {
        if (mode != climate::CLIMATE_MODE_COOL)
        {
            mode = climate::CLIMATE_MODE_COOL;
            changed = true;
        }
    }
    // auto mode
    else if (this->state_auto_.has_value() && validate(data, &state_auto_.value()))
    {
        if (mode != climate::CLIMATE_MODE_AUTO)
        {
            mode = climate::CLIMATE_MODE_AUTO;
            changed = true;
        }
    }
    // away
    if (this->state_away_.has_value())
    {
        bool is_away = *preset == climate::CLIMATE_PRESET_AWAY ? true : false;
        if (is_away != validate(data, &state_away_.value()))
        {
            *preset = is_away == true ? climate::CLIMATE_PRESET_HOME : climate::CLIMATE_PRESET_AWAY;
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

    if (changed) this->publish_state();
}

void UARTExClimate::control(const climate::ClimateCall &call)
{
    // Set mode
    if (call.get_mode().has_value() && mode != *call.get_mode())
    {
        mode = *call.get_mode();
        if (mode == climate::CLIMATE_MODE_OFF)
        {
            push_tx_cmd(this->get_command_off());
        }
        else if (mode == climate::CLIMATE_MODE_HEAT && this->command_heat_.has_value())
        {
            push_tx_cmd(&this->command_heat_.value());
        }
        else if (mode == climate::CLIMATE_MODE_COOL && this->command_cool_.has_value())
        {
            push_tx_cmd(&this->command_cool_.value());
        }
        else if (mode == climate::CLIMATE_MODE_AUTO)
        {
            if (this->command_auto_.has_value())
            {
                push_tx_cmd(&this->command_auto_.value());
            }
            else if (this->command_heat_.has_value() && this->command_cool_.has_value())
            {
                ESP_LOGW(TAG, "'%s' Auto mode not support.", get_name().c_str());
            }
            else if (this->command_heat_.has_value())
            {
                push_tx_cmd(&this->command_heat_.value());
                mode = climate::CLIMATE_MODE_HEAT;
            }
            else if (this->command_cool_.has_value())
            {
                push_tx_cmd(&this->command_cool_.value());
                mode = climate::CLIMATE_MODE_COOL;
            }
        }
    }

    // Set target temperature
    if (call.get_target_temperature().has_value() && this->target_temperature != *call.get_target_temperature())
    {
        this->target_temperature = *call.get_target_temperature();
        this->command_temperature_ = (this->command_temperature_func_)(this->target_temperature);
        push_tx_cmd(&this->command_temperature_);
    }

    // Set away
    if (this->command_away_.has_value() && call.get_preset().has_value() && *preset != *call.get_preset())
    {
        *preset = *call.get_preset();
        if (*preset == climate::CLIMATE_PRESET_AWAY)
        {
            push_tx_cmd(&this->command_away_.value());
        }
        else if (this->command_home_.has_value())
        {
            push_tx_cmd(&this->command_home_.value());
        }
        else if (mode == climate::CLIMATE_MODE_OFF)
        {
            push_tx_cmd(this->get_command_off());
        }
        else if (mode == climate::CLIMATE_MODE_HEAT && this->command_heat_.has_value())
        {
            push_tx_cmd(&this->command_heat_.value());
        }
        else if (mode == climate::CLIMATE_MODE_COOL && this->command_cool_.has_value())
        {
            push_tx_cmd(&this->command_cool_.value());
        }
        else if (mode == climate::CLIMATE_MODE_AUTO && this->command_auto_.has_value())
        {
            push_tx_cmd(&this->command_auto_.value());
        }
    }
    this->publish_state();
}

}  // namespace uartex
}  // namespace esphome
