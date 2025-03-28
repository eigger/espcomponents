#include "uartex_switch.h"
#include "esphome/core/log.h"

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
    if(state == this->state) return;
    enqueue_tx_cmd(state ? get_command_on() : get_command_off());
    publish_state(state);
}

}  // namespace uartex
}  // namespace esphome
