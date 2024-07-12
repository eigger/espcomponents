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
    if (has_state_func("state_template"))
    {
        optional<float> val = get_state_func("state_template", &data[0], data.size());
        if(val.has_value() && this->raw_state != val.value()) publish_state(val.value());
    }
    else if (get_state_num())
    {
        float val = state_to_float(data, *get_state_num());
        if(this->raw_state != val) publish_state(val);
    }
}

}  // namespace uartex
}  // namespace esphome
