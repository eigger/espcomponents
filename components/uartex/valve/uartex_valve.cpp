#include "uartex_valve.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.valve";

void UARTExValve::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Valve '%s':", get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExValve::setup()
{

}

void UARTExValve::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    if (this->state_position_func_.has_value())
    {
        optional<float> val = (*this->state_speed_func_)(&data[0], data.size());
        if (val.has_value() && this->position != (int)val.value())
        {
            this->position = (int)val.value();
            changed = true;
        }
    }
    if (this->state_open_.has_value() && verify_state(data, &this->state_open_.value()))
    {
        this->position = valve::VALVE_OPEN;
        changed = true;
    }
    else if (this->state_closed_.has_value() && verify_state(data, &this->state_closed_.value()))
    {
        this->position = valve::VALVE_CLOSED;
        changed = true;
    }
    if (changed) publish_state();
}

valve::ValveTraits UARTExValve::get_traits()
{
    valve::ValveTraits traits{};
    if (this->command_stop_.has_value()) traits.set_supports_stop(true);
    if (this->state_position_func_.has_value()) traits.set_supports_position(true);
    //traits.set_is_assumed_state(true);
    return traits;
}

void UARTExValve::control(const valve::ValveCall &call)
{
    if (*call.get_stop())
    {
        if (this->command_stop_.has_value()) enqueue_tx_cmd(&this->command_stop_.value());
        publish_state();
    }
    if (this->position != *call.get_position())
    {
        this->position = *call.get_position();
        if (this->position >= valve::VALVE_OPEN)
        {
            if (this->command_open_.has_value()) enqueue_tx_cmd(&this->command_open_.value());
        }
        else if (this->position <= valve::VALVE_CLOSED)
        {
            if (this->command_closed_.has_value()) enqueue_tx_cmd(&this->command_closed_.value());
        }
        publish_state();
    }
}

}  // namespace uartex
}  // namespace esphome
