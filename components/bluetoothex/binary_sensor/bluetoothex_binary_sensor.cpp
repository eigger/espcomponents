#include "bluetoothex_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bluetoothex {

static const char *TAG = "bluetoothex.binary_sensor";

void BluetoothExBinarySensor::dump_config()
{
    ESP_LOGCONFIG(TAG, "BluetoothEx Binary Sensor '%s':", device_name_->c_str());
    dump_bluetoothex_device_config(TAG);
}

void BluetoothExBinarySensor::publish(const std::vector<uint8_t>& data)
{
}

}  // namespace bluetoothex
}  // namespace esphome
