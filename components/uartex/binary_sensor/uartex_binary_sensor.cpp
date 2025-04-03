#include "uartex_binary_sensor.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.binary_sensor";

void UARTExBinarySensor::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    uartex_dump_config(TAG);
#endif
}

void UARTExBinarySensor::setup()
{
    publish_state(false);
}

}  // namespace uartex
}  // namespace esphome
