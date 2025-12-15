#include "uartex_text_sensor.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.text_sensor";

void UARTExTextSensor::dump_config() 
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    uartex_dump_config(TAG);
#endif
}

void UARTExTextSensor::setup()
{
}

void UARTExTextSensor::publish(const std::vector<uint8_t>& data) 
{
    optional<std::string> val = get_state_str("lambda", data);
    if(val.has_value() && this->get_raw_state() != val.value())
    {
        publish_state(val.value());
    }
}

}  // namespace uartex
}  // namespace esphome
