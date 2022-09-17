#include "uartex_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.text_sensor";

void UARTExTextSensor::dump_config() 
{
    ESP_LOGCONFIG(TAG, "UARTEx Text Sensor '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

void UARTExTextSensor::publish(const std::vector<uint8_t>& data) 
{
    if (this->f_.has_value())
    {
        optional<char*> val = (*this->f_)(&data[0], data.size());
        if(val.has_value() && this->raw_state != val.value()) this->publish_state(val.value());
    }
}

}  // namespace uartex
}  // namespace esphome
