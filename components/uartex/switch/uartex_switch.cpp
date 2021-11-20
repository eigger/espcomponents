#include "uartex_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.switch";

void UartExSwitch::dump_config()
{
    ESP_LOGCONFIG(TAG, "UartEx Switch '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

void UartExSwitch::publish(const uint8_t *data, const num_t len)
{
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(&data[0], len).c_str());
}

}  // namespace uartex
}  // namespace esphome
