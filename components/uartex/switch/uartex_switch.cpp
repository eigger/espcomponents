#include "uartex_switch.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.switch";

void UARTExSwitch::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    uartex_dump_config(TAG);
#endif
}

void UARTExSwitch::write_state(bool state)
{
    if (this->optimistic_ && this->has_published_state_ && this->state == (state != this->is_inverted())) return;
    // A previous request may still be pending or may have timed out. In
    // feedback mode, always accept an explicit command and let RX publish.
    if (enqueue_and_reflect(state ? get_command_on() : get_command_off())) publish(state);
}

}  // namespace uartex
}  // namespace esphome
