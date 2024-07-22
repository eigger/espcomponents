#include "uartex_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.sensor";

void UARTExSensor::dump_config() 
{
    ESP_LOGCONFIG(TAG, "UARTEx Sensor '%s':", get_name().c_str());
    uartex_dump_config(TAG);
}

void UARTExSensor::publish(const std::vector<uint8_t>& data) 
{
    optional<float> val = get_state_float("", data);
    if(val.has_value() && this->raw_state != val.value()) publish_state(val.value());
}

}  // namespace uartex
}  // namespace esphome
