#include "uartex_valve.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.valve";

void UARTExValve::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    log_config(TAG, "State Open", get_state_open());
    log_config(TAG, "State Closed", get_state_closed());
    log_config(TAG, "State Position", has_state_position());
    log_config(TAG, "Command Open", get_command_open());
    log_config(TAG, "Command Close", get_command_close());
    log_config(TAG, "Command Stop", get_command_stop());
    uartex_dump_config(TAG);
#endif
}

void UARTExValve::setup()
{
}

void UARTExValve::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    optional<float> val = get_state_position(data);
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
    if (has_state_position()) traits.set_supports_position(true);
    //traits.set_is_assumed_state(true);
    return traits;
}

void UARTExValve::control(const valve::ValveCall& call)
{
    if (call.get_stop()) enqueue_tx_cmd(get_command_stop());
    if (this->position != *call.get_position())
    {
        if (*call.get_position() >= valve::VALVE_OPEN)
        {
            if (enqueue_tx_cmd(get_command_open()) || this->optimistic_)
            {
                this->position = *call.get_position();
            }
        }
        else if (*call.get_position() <= valve::VALVE_CLOSED)
        {
            if (enqueue_tx_cmd(get_command_close()) || this->optimistic_)
            {
                this->position = *call.get_position();
            }
        }
    }
    publish_state();
}

}  // namespace uartex
}  // namespace esphome
