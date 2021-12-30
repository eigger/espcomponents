#include "rs485_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rs485 {

static const char *TAG = "rs485.binary_sensor";

void RS485BinarySensor::dump_config() {
    ESP_LOGCONFIG(TAG, "RS485 Binary Sensor '%s':", device_name_->c_str());
    dump_rs485_device_config(TAG);
}

void RS485BinarySensor::publish(const uint8_t *data, const num_t len) {
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(&data[0], len).c_str());
}

}  // namespace rs485
}  // namespace esphome
