#include "uartex_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.button";

void UARTExButton::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Button '%s':", get_name()->c_str());
    dump_uartex_device_config(TAG);
}

void UARTExButton::publish(const std::vector<uint8_t>& data)
{
}

}  // namespace uartex
}  // namespace esphome
