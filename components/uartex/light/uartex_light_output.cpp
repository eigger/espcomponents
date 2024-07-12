#include "uartex_light_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.light";

void UARTExLightOutput::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx LightOutput(Binary) '%s':", this->light_state_->get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExLightOutput::publish(const std::vector<uint8_t>& data)
{
    if (this->light_state_ == nullptr)return;
    if (has_state_func("state_brightness"))
    {
        optional<float> val = get_state_func("state_brightness", &data[0], data.size());
        if (val.has_value() && this->brightness_ != (int)val.value() && val.value() > 0)
        {
            this->brightness_ = (int)val.value();
            auto call = this->light_state_->make_call();
            call.set_brightness_if_supported(this->brightness_ / 100.0);
            call.set_state(this->state_);
            call.perform();
        }
    }
}

void UARTExLightOutput::publish_state(bool state)
{
    if (this->light_state_ == nullptr || state == this->state_) return;
    this->state_ = state;
    auto call = this->light_state_->make_call();
    call.set_brightness_if_supported(this->brightness_ / 100.0);
    call.set_state(state);
    call.perform();
}

light::LightTraits UARTExLightOutput::get_traits()
{
    auto traits = light::LightTraits();
    std::set<light::ColorMode> color_modes;
    if (get_command_brightness(this->brightness_) || has_state_func("state_brightness"))
    {
        color_modes.insert(light::ColorMode::BRIGHTNESS);
    }
    else
    {
        color_modes.insert(light::ColorMode::ON_OFF);
    }
    traits.set_supported_color_modes(color_modes);
    return traits;
}

void UARTExLightOutput::write_state(light::LightState *state)
{
    bool binary;
    state->current_values_as_binary(&binary);
    if (binary != this->state_)
    {
        enqueue_tx_cmd(binary ? get_command_on() : get_command_off());
        this->state_ = binary;
    }
    if (get_command_brightness(this->brightness_))
    {
        float brightness;
        state->current_values_as_brightness(&brightness);
        if ((int)(brightness * 100.0) != this->brightness_ && brightness > 0)
        {
            this->brightness_ = (int)(brightness * 100.0);
            enqueue_tx_cmd(get_command_brightness(this->brightness_));
        }
    }
    this->light_state_ = state;
}

cmd_t *UARTExLightOutput::get_command_brightness(const float x)
{
    return get_command("command_brightness", x);
}

}  // namespace uartex
}  // namespace esphome
