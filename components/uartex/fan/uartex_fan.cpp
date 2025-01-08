#include "uartex_fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.fan";

void UARTExFan::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Fan '%s':", get_name().c_str());
    uartex_dump_config(TAG);
}

fan::FanTraits UARTExFan::get_traits()
{
    fan::FanTraits traits{};
    if (this->speed_count_ > 0)
    {
        traits.set_speed(true);
        traits.set_supported_speed_count(this->speed_count_);
    }
    if (!this->preset_modes_.empty()) traits.set_supported_preset_modes(this->preset_modes_);
    return traits;
}

void UARTExFan::publish(const std::vector<uint8_t>& data)
{
    optional<float> val = get_state_speed(data);
    if (val.has_value() && this->speed != (int)val.value())
    {
        this->speed = (int)val.value();
        publish_state();
    }
    optional<const char*> preset = get_state_preset(data);
    if(preset.has_value() && this->preset_mode != preset.value())
    {
        this->preset_mode = preset.value();
        publish_state();
    }
    
}

void UARTExFan::publish(const bool state)
{
    if (state == this->state) return;
    this->state = state; 
    this->publish_state();
}

void UARTExFan::control(const fan::FanCall& call)
{
    bool changed_state = false;
    bool changed_speed = false;
    bool changed_oscillating = false;
    bool changed_direction = false;
    bool changed_preset = false;
    if (call.get_state().has_value() && this->state != *call.get_state())
    {
        this->state = *call.get_state();
        changed_state = true;
    }
    if (call.get_oscillating().has_value() && this->oscillating != *call.get_oscillating())
    {
        this->oscillating = *call.get_oscillating();
        changed_oscillating = true;
    }
    if (call.get_speed().has_value() && this->speed != *call.get_speed())
    {
        this->speed = *call.get_speed();
        changed_speed = true;
    }
    if (call.get_direction().has_value() && this->direction != *call.get_direction())
    {
        this->direction = *call.get_direction();
        changed_direction = true;
    }
    if (call.get_preset_mode().size() > 0 && this->preset_mode != call.get_preset_mode())
    {
        this->preset_mode = call.get_preset_mode();
        changed_preset = true;
    }
    if (changed_preset) enqueue_tx_cmd(get_command_preset(this->preset_mode));
    if (this->state && changed_state) enqueue_tx_cmd(get_command_on());
    if (changed_speed) enqueue_tx_cmd(get_command_speed(this->speed));
    if (!this->state && changed_state) enqueue_tx_cmd(get_command_off());
    publish_state();
}

}  // namespace uartex
}  // namespace esphome