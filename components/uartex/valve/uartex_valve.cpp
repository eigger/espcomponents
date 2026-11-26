#include "uartex_valve.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.valve";

void UARTExValve::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", uartex_entity_name(*this).c_str());
    log_config(TAG, "State Open", get_state_open());
    log_config(TAG, "State Closed", get_state_closed());
    log_config(TAG, "State Position", has_state_position());
    log_config(TAG, "Command Open", get_command_open());
    log_config(TAG, "Command Close", get_command_close());
    log_config(TAG, "Command Stop", get_command_stop());
    log_config(TAG, "Command Position", has_command_position());
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
    if (val.has_value() && this->position != val.value())
    {
        this->position = val.value();
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
    if (has_state_position() || has_command_position()) traits.set_supports_position(true);
    //traits.set_is_assumed_state(true);
    return traits;
}

void UARTExValve::control(const valve::ValveCall& call)
{
    if (call.get_stop()) enqueue_tx_cmd(get_command_stop());
    if (call.get_position().has_value())
    {
        const float pos = *call.get_position();
        // Fully open/closed use command_open/command_close; fall back to command_position when those are absent
        if (pos == valve::VALVE_OPEN && (has_named_command("command_open") || !has_command_position()))
        {
            if (enqueue_and_reflect(get_command_open())) this->position = pos;
        }
        else if (pos == valve::VALVE_CLOSED && (has_named_command("command_close") || !has_command_position()))
        {
            if (enqueue_and_reflect(get_command_close())) this->position = pos;
        }
        else
        {
            if (enqueue_and_reflect(get_command_position(pos))) this->position = pos;
        }
    }
    publish_state();
}

}  // namespace uartex
}  // namespace esphome
