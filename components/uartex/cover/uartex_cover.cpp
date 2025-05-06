#include "uartex_cover.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.cover";

void UARTExCover::dump_config()
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

void UARTExCover::setup()
{
}

void UARTExCover::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    optional<float> pos = get_state_position(data);
    if (pos.has_value() && this->position != pos.value())
    {
        this->position = pos.value();
        changed = true;
    }
    optional<float> tilt = get_state_tilt(data);
    if (tilt.has_value() && this->tilt != tilt.value())
    {
        this->tilt = tilt.value();
        changed = true;
    }
    if (verify_state(data, get_state_open()))
    {
        this->position = cover::COVER_OPEN;
        changed = true;
    }
    else if (verify_state(data, get_state_closed()))
    {
        this->position = cover::COVER_CLOSED;
        changed = true;
    }
    if (changed) publish_state();
}

cover::CoverTraits UARTExCover::get_traits()
{
    cover::CoverTraits traits{};
    if (get_command_stop()) traits.set_supports_stop(true);
    if (has_state_position()) traits.set_supports_position(true);
    if (has_state_tilt()) traits.set_supports_tilt(true);
    return traits;
}

void UARTExCover::control(const cover::CoverCall &call)
{
    if (call.get_stop()) enqueue_tx_cmd(get_command_stop());
    if (call.get_position().has_value())
    {
        auto pos = *call.get_position();
        if (pos == cover::COVER_OPEN)
        {
            if (enqueue_tx_cmd(get_command_open()) || this->optimistic_)
            {
                this->position = pos;
            }
        }
        else if (pos == cover::COVER_CLOSED)
        {
            if (enqueue_tx_cmd(get_command_close()) || this->optimistic_)
            {
                this->position = pos;
            }
        }
        else
        {
            if (enqueue_tx_cmd(get_command_position(pos)) || this->optimistic_)
            {
                this->position = pos;
            }
        }
    }
    if (call.get_tilt().has_value())
    {
        auto tilt = *call.get_tilt();
        if (enqueue_tx_cmd(get_command_tilt(tilt)) || this->optimistic_)
        {
            this->tilt = tilt;
        }
    }
    publish_state();
}

}  // namespace uartex
}  // namespace esphome
