#include "uartex_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.text_sensor";

void UARTExTextSensor::dump_config() 
{
    ESP_LOGCONFIG(TAG, "UARTEx Text Sensor '%s':", get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExTextSensor::setup()
{
    publish_state("");
}

void UARTExTextSensor::publish(const std::vector<uint8_t>& data) 
{
    optional<const char*> val = get_state_str("", data);
    if(val.has_value() && this->raw_state != val.value())
    {
        publish_state(val.value());
    }
}

}  // namespace uartex
}  // namespace esphome
