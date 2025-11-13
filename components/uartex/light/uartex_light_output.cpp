#include "uartex_light_output.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.light";

void UARTExLightOutput::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", this->light_state_->get_name().c_str());
    log_config(TAG, "State Brightness", get_state_num("state_brightness"));
    uartex_dump_config(TAG);
#endif
}

void UARTExLightOutput::publish(const std::vector<uint8_t>& data)
{
    if (this->light_state_ == nullptr)return;
    optional<float> val = get_state_brightness(data);
    if (val.has_value() && this->brightness_ != (int)val.value() && val.value() > 0)
    {
        this->brightness_ = (int)val.value();
        auto call = this->light_state_->make_call();
        call.set_brightness_if_supported(this->brightness_ / 100.0);
        call.set_state(this->state_);
        call.perform();
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
    if (get_command_brightness(this->brightness_) || has_state_brightness())
    {
        traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    }
    else
    {
        traits.set_supported_color_modes({light::ColorMode::ON_OFF});
    }
    return traits;
}

void UARTExLightOutput::write_state(light::LightState* state)
{
    bool binary = false;
    state->current_values_as_binary(&binary);
    if (binary != this->state_)
    {
        if (enqueue_tx_cmd(binary ? get_command_on() : get_command_off()) || this->optimistic_)
        {
            this->state_ = binary;
        }
        
    }
    float brightness = 0;
    state->current_values_as_brightness(&brightness);
    brightness *= 100;
    if ((int)brightness != this->brightness_ && brightness > 0)
    {
        if (enqueue_tx_cmd(get_command_brightness(brightness)) || this->optimistic_)
        {
            this->brightness_ = (int)brightness;
        }
    }
    this->light_state_ = state;
}

}  // namespace uartex
}  // namespace esphome
