#include "bluetoothex_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bluetoothex {

static const char *TAG = "bluetoothex.button";

void BluetoothExButton::dump_config()
{
    ESP_LOGCONFIG(TAG, "BluetoothEx Button '%s':", device_name_->c_str());
    dump_bluetoothex_device_config(TAG);
}

void BluetoothExButton::publish(const std::vector<uint8_t>& data)
{
}

}  // namespace bluetoothex
}  // namespace esphome
