#include "bluetoothex_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bluetoothex {

static const char *TAG = "bluetoothex.sensor";

void BluetoothExSensor::dump_config() 
{
    ESP_LOGCONFIG(TAG, "BluetoothEx Sensor '%s':", device_name_->c_str());
    dump_bluetoothex_device_config(TAG);
}

void BluetoothExSensor::publish(const std::vector<uint8_t>& data) 
{
    if (this->f_.has_value())
    {
        optional<float> val = (*this->f_)(&data[0], data.size());
        if(val.has_value() && this->raw_state != val.value()) this->publish_state(val.value());
    }
    else if(this->conf_state_num_.has_value() && data.size() >= (this->conf_state_num_.value().offset + this->conf_state_num_.value().length)) 
    {
        float val = state_to_float(data, this->conf_state_num_.value());
        if(this->raw_state != val) this->publish_state(val);
    }
}

}  // namespace bluetoothex
}  // namespace esphome
