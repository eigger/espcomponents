#include "bluetoothex_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bluetoothex {

static const char *TAG = "bluetoothex.switch";

void BluetoothExSwitch::dump_config()
{
    ESP_LOGCONFIG(TAG, "BluetoothEx Switch '%s':", device_name_->c_str());
    dump_bluetoothex_device_config(TAG);
}

void BluetoothExSwitch::publish(const std::vector<uint8_t>& data)
{
}

}  // namespace bluetoothex
}  // namespace esphome
