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

void WallPadBinarySensor::publish(const std::vector<uint8_t>& data)
{
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), to_hex_string(data).c_str());
}

}  // namespace wallpad
}  // namespace esphome
