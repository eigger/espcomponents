#include "uartex_sensor.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.sensor";

void UARTExSensor::dump_config() 
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    log_config(TAG, "State Number", get_state_num("state_number"));
    uartex_dump_config(TAG);
#endif
}

void UARTExSensor::publish(const std::vector<uint8_t>& data) 
{
    {
        optional<float> val = get_state_float("lambda", data);
        if(val.has_value() && this->raw_state != val.value()) publish_state(val.value());
    }

    {
        optional<float> val = get_state_number(data);
        if(val.has_value() && this->raw_state != val.value()) publish_state(val.value());
    }
}

}  // namespace uartex
}  // namespace esphome
