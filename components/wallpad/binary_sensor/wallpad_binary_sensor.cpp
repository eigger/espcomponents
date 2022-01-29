#include "wallpad_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wallpad {

static const char *TAG = "wallpad.binary_sensor";

void WallPadBinarySensor::dump_config()
{
    ESP_LOGCONFIG(TAG, "WallPad Binary Sensor '%s':", device_name_->c_str());
    dump_wallpad_device_config(TAG);
}

void WallPadBinarySensor::publish(const uint8_t *data, const num_t len)
{
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(&data[0], len).c_str());
}

}  // namespace wallpad
}  // namespace esphome
