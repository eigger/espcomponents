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
    if (get_state_off() && verify_state(data, get_state_off()))
    {
        this->mode = climate::CLIMATE_MODE_OFF;
        changed = true;
    }
    else
    {
        for(const auto& state : this->state_mode_)
        {
            if (verify_state(data, &state.second.value()))
            {
                if (this->mode != state.first)
                {
                    this->mode = state.first;
                    changed = true;
                }
            }
        }
    }

    //Swing Mode
    for(const auto& state : this->state_swing_mode_)
    {
        if (verify_state(data, &state.second.value()))
        {
            if (this->swing_mode != state.first)
            {
                this->swing_mode = state.first;
                changed = true;
            }
        }
    }

    //Fan Mode
    for(const auto& state : this->state_fan_mode_)
    {
        if (verify_state(data, &state.second.value()))
        {
            if (this->fan_mode != state.first)
            {
                this->fan_mode = state.first;
                changed = true;
            }
        }
    }

    //Preset
    for(const auto& state : this->state_preset_)
    {
        if (verify_state(data, &state.second.value()))
        {
            if (this->preset != state.first)
            {
                this->preset = state.first;
                changed = true;
            }
        }
    }
    
    // Current temperature
    if (this->sensor_ == nullptr)
    {
        if (this->state_current_temperature_func_.has_value())
        {
            optional<float> val = (*this->state_current_temperature_func_)(&data[0], data.size());
            if (val.has_value() && this->current_temperature != val.value())
            {
                this->current_temperature = val.value();
                changed = true;
            }
        }
        else if (this->state_current_temperature_.has_value() && data.size() >= (this->state_current_temperature_.value().offset + this->state_current_temperature_.value().length))
        {
            float val = state_to_float(data, this->state_current_temperature_.value());
            if (this->current_temperature != val)
            {
                this->current_temperature = val;
                changed = true;
            }
        }
    }

    // Target temperature
    if (this->state_target_temperature_func_.has_value())
    {
        optional<float> val = (*this->state_target_temperature_func_)(&data[0], data.size());
        if (val.has_value() && this->target_temperature != val.value())
        {
            this->target_temperature = val.value();
            changed = true;
        }
    }
    else if (this->state_target_temperature_.has_value() && data.size() >= (this->state_target_temperature_.value().offset + this->state_target_temperature_.value().length))
    {
        float val = state_to_float(data, this->state_target_temperature_.value());
        if (this->target_temperature != val)
        {
            this->target_temperature = val;
            changed = true;
        }
    }

    // Current humidity
    if (this->state_current_humidity_func_.has_value())
    {
        optional<float> val = (*this->state_current_humidity_func_)(&data[0], data.size());
        if (val.has_value() && this->current_humidity != val.value())
        {
            this->current_humidity = val.value();
            changed = true;
        }
    }
    else if (this->state_current_humidity_.has_value() && data.size() >= (this->state_current_humidity_.value().offset + this->state_current_humidity_.value().length))
    {
        float val = state_to_float(data, this->state_current_humidity_.value());
        if (this->current_humidity != val)
        {
            this->current_humidity = val;
            changed = true;
        }
    }

    // Target humidity
    if (this->state_target_humidity_func_.has_value())
    {
        optional<float> val = (*this->state_target_humidity_func_)(&data[0], data.size());
        if (val.has_value() && this->target_humidity != val.value())
        {
            this->target_humidity = val.value();
            changed = true;
        }
    }
    else if (this->state_target_humidity_.has_value() && data.size() >= (this->state_target_humidity_.value().offset + this->state_target_humidity_.value().length))
    {
        float val = state_to_float(data, this->state_target_humidity_.value());
        if (this->target_humidity != val)
        {
            this->target_humidity = val;
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
        else
        {
            if (command_mode_func_.find(this->mode) != command_mode_func_.end())
            {
                this->command_mode_[this->mode] = (this->command_mode_func_[this->mode])();
                enqueue_tx_cmd(&this->command_mode_[this->mode]);
            }
            else if (command_mode_.find(this->mode) != command_mode_.end())
            {
                enqueue_tx_cmd(&this->command_mode_[this->mode]);
            }
        }
    }

    // Set target temperature
    if (call.get_target_temperature().has_value() && this->target_temperature != *call.get_target_temperature())
    {
        this->target_temperature = *call.get_target_temperature();
        if (this->command_temperature_func_ != nullptr)
        {
            this->command_temperature_ = (this->command_temperature_func_)(this->target_temperature);
            enqueue_tx_cmd(&this->command_temperature_);
        }
    }

    // Set target humidity
    if (call.get_target_humidity().has_value() && this->target_humidity != *call.get_target_humidity())
    {
        this->target_humidity = *call.get_target_humidity();
        if (this->command_humidity_func_ != nullptr)
        {
            this->command_humidity_ = (this->command_humidity_func_)(this->target_humidity);
            enqueue_tx_cmd(&this->command_humidity_);
        }
    }

    // Set swing mode
    if (call.get_swing_mode().has_value() && this->swing_mode != *call.get_swing_mode())
    {
        this->swing_mode = *call.get_swing_mode();
        if (command_swing_mode_func_.find(this->swing_mode) != command_swing_mode_func_.end())
        {
            this->command_swing_mode_[this->swing_mode] = (this->command_swing_mode_func_[this->swing_mode])();
            enqueue_tx_cmd(&this->command_swing_mode_[this->swing_mode]);
        }
        else if (command_swing_mode_.find(this->swing_mode) != command_swing_mode_.end())
        {
            enqueue_tx_cmd(&this->command_swing_mode_[this->swing_mode]);
        }
    }

    // Set fan mode
    if (call.get_fan_mode().has_value() && this->fan_mode != *call.get_fan_mode())
    {
        this->fan_mode = *call.get_fan_mode();
        if (command_fan_mode_func_.find(this->fan_mode.value()) != command_fan_mode_func_.end())
        {
            this->command_fan_mode_[this->fan_mode.value()] = (this->command_fan_mode_func_[this->fan_mode.value()])();
            enqueue_tx_cmd(&this->command_fan_mode_[this->fan_mode.value()]);
        }
        else if (command_fan_mode_.find(this->fan_mode.value()) != command_fan_mode_.end())
        {
            enqueue_tx_cmd(&this->command_fan_mode_[this->fan_mode.value()]);
        }
    }

    // Set preset
    if (call.get_preset().has_value() && this->preset != *call.get_preset())
    {
        this->preset = *call.get_preset();
        if (command_preset_func_.find(this->preset.value()) != command_preset_func_.end())
        {
            this->command_preset_[this->preset.value()] = (this->command_preset_func_[this->preset.value()])();
            enqueue_tx_cmd(&this->command_preset_[this->preset.value()]);
        }
        else if (command_preset_.find(this->preset.value()) != command_preset_.end())
        {
            enqueue_tx_cmd(&this->command_preset_[this->preset.value()]);
        }
    }
    publish_state();
}

}  // namespace uartex
}  // namespace esphome
