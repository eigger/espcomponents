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
        else
        {
            for(const auto& command : this->command_mode_)
            {
                if (command.first == this->mode)
                {
                    enqueue_tx_cmd(&command.second.value());
                    break;
                }
            }
        }
    }

    // Set target temperature
    if (call.get_target_temperature().has_value() && this->target_temperature != *call.get_target_temperature())
    {
        this->target_temperature = *call.get_target_temperature();
        this->command_temperature_ = (this->command_temperature_func_)(this->target_temperature, this->mode, this->preset);
        enqueue_tx_cmd(&this->command_temperature_);
    }

    // Set swing mode
    if (call.get_swing_mode().has_value() && this->swing_mode != *call.get_swing_mode())
    {
        this->swing_mode = *call.get_swing_mode();
        for(const auto& command : this->command_swing_mode_)
        {
            if (command.first == this->swing_mode)
            {
                enqueue_tx_cmd(&command.second.value());
                break;
            }
        }
    }

    // Set preset
    if (call.get_preset().has_value() && this->preset != *call.get_preset())
    {
        this->preset = *call.get_preset();
        for(const auto& command : this->command_preset_)
        {
            if (command.first == this->preset)
            {
                enqueue_tx_cmd(&command.second.value());
                break;
            }
        }
    }
    publish_state();
}

}  // namespace uartex
}  // namespace esphome
