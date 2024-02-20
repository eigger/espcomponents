#include "uartex_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.sensor";

void UARTExSensor::dump_config() 
{
    ESP_LOGCONFIG(TAG, "UARTEx Sensor '%s':", get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExSensor::publish(const std::vector<uint8_t>& data) 
{
    UARTExDevice::publish(data);
    if (this->f_.has_value())
    {
        optional<float> val = (*this->f_)(&data[0], data.size());
        if(val.has_value() && this->raw_state != val.value()) publish_state(val.value());
    }
    else if(this->conf_state_num_.has_value() && data.size() >= (this->conf_state_num_.value().offset + this->conf_state_num_.value().length)) 
    {
        float val = state_to_float(data, this->conf_state_num_.value());
        if(this->raw_state != val) publish_state(val);
    }
}

}  // namespace uartex
}  // namespace esphome
