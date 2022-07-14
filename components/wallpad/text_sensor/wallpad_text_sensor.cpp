#include "wallpad_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wallpad {

static const char *TAG = "wallpad.text_sensor";

void WallPadTextSensor::dump_config() 
{
    ESP_LOGCONFIG(TAG, "WallPad Text Sensor '%s':", device_name_->c_str());
    dump_wallpad_device_config(TAG);
}

void WallPadTextSensor::publish(const std::vector<uint8_t>& data) 
{
    if (this->f_.has_value())
    {
        optional<std::string> val = (*this->f_)(&data[0], data.size());
        if(val.has_value() && this->raw_state != val.value()) this->publish_state(val.value());
    }
}

}  // namespace wallpad
}  // namespace esphome
