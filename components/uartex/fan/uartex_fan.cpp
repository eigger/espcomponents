#include "uartex_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.fan";

void UARTExFan::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Fan '%s':", get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExFan::setup()
{
}

void UARTExFan::control(const fan::FanCall &call)
{
    bool changed_state = false;
    bool changed_speed = false;
    bool changed_oscillating = false;
    bool changed_direction = false;
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
      
    if (command_on_.has_value() && this->state && changed_state) push_tx_cmd(&this->command_on_.value());
    if (changed_speed)
    {
        if (this->command_speed_func_.has_value())
        {
            command_speed_ = (*this->command_speed_func_)((float)this->speed);
            push_tx_cmd(&command_speed_);
        }
    }
    if (command_off_.has_value() && !this->state && changed_state) push_tx_cmd(&this->command_off_.value());
    this->publish_state();
}

void UARTExFan::publish(const std::vector<uint8_t>& data)
{
    if (this->state_speed_func_.has_value())
    {
        optional<float> val = (*this->state_speed_func_)(&data[0], data.size());
        if (val.has_value() && this->speed != (int)val.value())
        {
            this->speed = (int)val.value();
            this->publish_state();
        }
    }
}

}  // namespace uartex
}  // namespace esphome