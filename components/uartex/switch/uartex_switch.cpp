#include "uartex_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.switch";

void UARTExSwitch::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Switch '%s':", get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExSwitch::write_state(bool state)
{
    if(state == this->state) return;
    enqueue_tx_cmd(state ? get_command_on() : get_command_off());
    publish_state(state);
}

}  // namespace uartex
}  // namespace esphome
