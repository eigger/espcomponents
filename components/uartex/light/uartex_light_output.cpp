#include "uartex_light_output.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"


namespace esphome {
namespace uartex {

static const char *TAG = "uartex.light";

void UARTExLightOutput::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx LightOutput(Binary) '%s':", light_state_->get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExLightOutput::publish(const std::vector<uint8_t>& data)
{
    if (this->state_brightness_func_.has_value())
    {
        optional<float> val = (*this->state_brightness_func_)(&data[0], data.size());
        if (val.has_value() && this->brightness_ != (int)val.value())
        {
            this->brightness_ = (int)val.value();
            auto call = light_state_->make_call();
            call.set_brightness(this->brightness_);
            call.perform();
        }
    }
}

void UARTExLightOutput::publish_state(bool state)
{
    if (light_state_ == nullptr || state == this->state_)return;
    this->state_ = state;
    auto call = light_state_->make_call();
    call.set_state(state);
    call.perform();
}

light::LightTraits UARTExLightOutput::get_traits()
{
    auto traits = light::LightTraits();
    traits.supports_color_capability(light::ColorCapability::BRIGHTNESS);
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
    if (this->command_brightness_func_.has_value())
    {
        float brightness;
        state->current_values_as_brightness(&brightness);
        if ((int)brightness != this->brightness_)
        {
            this->brightness_ = (int)brightness;
            enqueue_tx_cmd(get_command_brightness());
        }
    }
}

cmd_t *UARTExLightOutput::get_command_brightness()
{
    if (this->command_brightness_func_.has_value())
        this->command_brightness_ = (*this->command_brightness_func_)(this->brightness_);
    return &this->command_brightness_.value();
}

}  // namespace uartex
}  // namespace esphome
