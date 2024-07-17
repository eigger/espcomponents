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
    optional<float> val = get_state_num("state_position", data);
    if (val.has_value() && this->position != (int)val.value())
    {
        this->position = (int)val.value();
        changed = true;
    }
    if (verify_state(data, get_state_open()))
    {
        this->position = valve::VALVE_OPEN;
        changed = true;
    }
    else if (verify_state(data, get_state_closed()))
    {
        this->position = valve::VALVE_CLOSED;
        changed = true;
    }
    if (changed) publish_state();
}

valve::ValveTraits UARTExValve::get_traits()
{
    valve::ValveTraits traits{};
    if (get_command_stop()) traits.set_supports_stop(true);
    if (has_state_func("state_position")) traits.set_supports_position(true);
    //traits.set_is_assumed_state(true);
    return traits;
}

void UARTExValve::control(const valve::ValveCall &call)
{
    if (call.get_stop())
    {
        enqueue_tx_cmd(get_command_stop());
        publish_state();
    }
    if (this->position != *call.get_position())
    {
        this->position = *call.get_position();
        if (this->position >= valve::VALVE_OPEN)
        {
            enqueue_tx_cmd(get_command_open());
        }
        else if (this->position <= valve::VALVE_CLOSED)
        {
            enqueue_tx_cmd(get_command_close());
        }
        publish_state();
    }
}

}  // namespace uartex
}  // namespace esphome
