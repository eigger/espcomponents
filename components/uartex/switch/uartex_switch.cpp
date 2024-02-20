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

}  // namespace uartex
}  // namespace esphome
