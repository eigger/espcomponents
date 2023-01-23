#include "bluetoothex_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bluetoothex {

static const char *TAG = "bluetoothex.text_sensor";

void BluetoothExTextSensor::dump_config() 
{
    ESP_LOGCONFIG(TAG, "BluetoothEx Text Sensor '%s':", device_name_->c_str());
    dump_bluetoothex_device_config(TAG);
}

void BluetoothExTextSensor::publish(const std::vector<uint8_t>& data) 
{
    if (this->f_.has_value())
    {
        optional<const char*> val = (*this->f_)(&data[0], data.size());
        if(val.has_value() && this->raw_state != val.value()) this->publish_state(val.value());
    }
}

}  // namespace bluetoothex
}  // namespace esphome
