#include "wallpad_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wallpad {

static const char *TAG = "wallpad.sensor";

void WallPadSensor::dump_config() 
{
    ESP_LOGCONFIG(TAG, "WallPad Sensor '%s':", device_name_->c_str());
    dump_wallpad_device_config(TAG);
}

void WallPadSensor::publish(const uint8_t *data, const num_t len) 
{
    if (this->f_.has_value())
    {
        optional<float> val = (*this->f_)(data, len);
        if(val.has_value() && this->raw_state != val.value()) this->publish_state(val.value());
    }
    else if(this->conf_state_num_.has_value() && len >= (this->conf_state_num_.value().offset + this->conf_state_num_.value().length)) 
    {
        float val = hex_to_float(&data[this->conf_state_num_.value().offset], this->conf_state_num_.value().length, this->conf_state_num_.value().precision);
        if(this->raw_state != val) this->publish_state(val);
    }
}

}  // namespace wallpad
}  // namespace esphome
