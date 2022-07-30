#include "uartex_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.switch";

void UARTExSwitch::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Switch '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

void UARTExSwitch::publish(const std::vector<uint8_t>& data)
{
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), to_hex_string(data).c_str());
}

}  // namespace uartex
}  // namespace esphome
