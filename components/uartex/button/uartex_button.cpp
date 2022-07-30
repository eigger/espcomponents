#include "uartex_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.button";

void UARTExButton::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Button '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

void UARTExButton::publish(const std::vector<uint8_t>& data)
{
    //ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), to_hex_string(&data[0], len).c_str());
}

}  // namespace uartex
}  // namespace esphome
