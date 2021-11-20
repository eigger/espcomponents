#include "uartex_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.climate";

void UartExClimate::dump_config()
{
    ESP_LOGCONFIG(TAG, "UartEx Climate '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

climate::ClimateTraits UartExClimate::traits()
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

void UartExClimate::setup()
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

void UartExClimate::publish(const uint8_t *data, const num_t len)
{
    bool changed = false;
    climate::ClimateCall mcall = make_call();
    // turn off
    if (this->state_off_.has_value() && compare(&data[0], len, &state_off_.value()))
    {
        if (mcall.get_mode() != climate::CLIMATE_MODE_OFF)
        {
            mcall.set_mode(climate::CLIMATE_MODE_OFF);
            changed = true;
        }
    }
    // heat mode
    else if (this->state_heat_.has_value() && compare(&data[0], len, &state_heat_.value()))
    {
        if (mcall.get_mode() != climate::CLIMATE_MODE_HEAT)
        {
            mcall.set_mode(climate::CLIMATE_MODE_HEAT);
            changed = true;
        }
    }
    // cool mode
    else if (this->state_cool_.has_value() && compare(&data[0], len, &state_cool_.value()))
    {
        if (mcall.get_mode() != climate::CLIMATE_MODE_COOL)
        {
            mcall.set_mode(climate::CLIMATE_MODE_COOL);
            changed = true;
        }
    }
    // auto mode
    else if (this->state_auto_.has_value() && compare(&data[0], len, &state_auto_.value()))
    {
        if (mcall.get_mode() != climate::CLIMATE_MODE_AUTO)
        {
            mcall.set_mode(climate::CLIMATE_MODE_AUTO);
            changed = true;
        }
    }
    // away
    if (this->state_away_.has_value())
    {
        bool is_away = mcall.get_preset() == climate::CLIMATE_PRESET_AWAY ? true : false;
        if (is_away != compare(&data[0], len, &state_away_.value()))
        {
            mcall.set_preset(is_away == true ? climate::CLIMATE_PRESET_HOME : climate::CLIMATE_PRESET_AWAY);
            changed = true;
        }
    }

    // Current temperature
    if (this->sensor_ == nullptr)
    {
        if (this->state_current_func_.has_value())
        {
            optional<float> val = (*this->state_current_func_)(data, len);
            if (val.has_value() && this->current_temperature != val.value())
            {
                this->current_temperature = val.value();
                changed = true;
            }
        }
        else if (this->state_current_.has_value() && len >= (this->state_current_.value().offset + this->state_current_.value().length))
        {
            float val = hex_to_float(&data[this->state_current_.value().offset], this->state_current_.value().length, this->state_current_.value().precision);
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
        optional<float> val = (*this->state_target_func_)(data, len);
        if (val.has_value() && this->target_temperature != val.value())
        {
            this->target_temperature = val.value();
            changed = true;
        }
    }
    else if (this->state_target_.has_value() && len >= (this->state_target_.value().offset + this->state_target_.value().length))
    {
        float val = hex_to_float(&data[this->state_target_.value().offset], this->state_target_.value().length, this->state_target_.value().precision);
        if (this->target_temperature != val)
        {
            this->target_temperature = val;
            changed = true;
        }
    }

    if (changed) this->publish_state();
}

void UartExClimate::control(const climate::ClimateCall &call)
{
    // Set mode
    climate::ClimateCall mcall = make_call();
    if (call.get_mode().has_value() && mcall.get_mode() != *call.get_mode())
    {
        mcall.set_mode(*call.get_mode());
        if (mcall.get_mode() == climate::CLIMATE_MODE_OFF)
        {
            write_with_header(this->get_command_off());
        }
        else if (mcall.get_mode() == climate::CLIMATE_MODE_HEAT && this->command_heat_.has_value())
        {
            write_with_header(&this->command_heat_.value());
        }
        else if (mcall.get_mode() == climate::CLIMATE_MODE_COOL && this->command_cool_.has_value())
        {
            write_with_header(&this->command_cool_.value());
        }
        else if (mcall.get_mode() == climate::CLIMATE_MODE_AUTO)
        {
            if (this->command_auto_.has_value())
            {
                write_with_header(&this->command_auto_.value());
            }
            else if (this->command_heat_.has_value() && this->command_cool_.has_value())
            {
                ESP_LOGW(TAG, "'%s' Auto mode not support.", this->device_name_->c_str());
            }
            else if (this->command_heat_.has_value())
            {
                write_with_header(&this->command_heat_.value());
                mcall.set_mode(climate::CLIMATE_MODE_HEAT);
            }
            else if (this->command_cool_.has_value())
            {
                write_with_header(&this->command_cool_.value());
                mcall.set_mode(climate::CLIMATE_MODE_COOL);
            }
        }
    }

    // Set target temperature
    if (call.get_target_temperature().has_value() && this->target_temperature != *call.get_target_temperature())
    {
        this->target_temperature = *call.get_target_temperature();
        this->command_temperature_ = (this->command_temperature_func_)(this->target_temperature);
        write_with_header(&this->command_temperature_);
    }

    // Set away
    if (this->command_away_.has_value() && call.get_away().has_value() && mcall.get_preset() != *call.get_preset())
    {
        mcall.set_preset(*call.get_preset());
        if (mcall.get_preset() == climate::CLIMATE_PRESET_AWAY)
        {
            write_with_header(&this->command_away_.value());
        }
        else if (this->command_home_.has_value())
        {
            write_with_header(&this->command_home_.value());
        }
        else if (mcall.get_mode() == climate::CLIMATE_MODE_OFF)
        {
            write_with_header(this->get_command_off());
        }
        else if (mcall.get_mode() == climate::CLIMATE_MODE_HEAT && this->command_heat_.has_value())
        {
            write_with_header(&this->command_heat_.value());
        }
        else if (mcall.get_mode() == climate::CLIMATE_MODE_COOL && this->command_cool_.has_value())
        {
            write_with_header(&this->command_cool_.value());
        }
        else if (mcall.get_mode() == climate::CLIMATE_MODE_AUTO && this->command_auto_.has_value())
        {
            write_with_header(&this->command_auto_.value());
        }
    }
    this->publish_state();
}

}  // namespace uartex
}  // namespace esphome
