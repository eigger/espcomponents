#include "uartex_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.binary_sensor";

void UartExBinarySensor::dump_config() {
    ESP_LOGCONFIG(TAG, "UartEx Binary Sensor '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

void UartExBinarySensor::publish(const uint8_t *data, const num_t len) {
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(&data[0], len).c_str());
}

}  // namespace uartex
}  // namespace esphome
