#include "uartex_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.binary_sensor";

void UARTExBinarySensor::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Binary Sensor '%s':", get_name().c_str());
    uartex_dump_config(TAG);
}

void UARTExBinarySensor::setup()
{
    publish_state(false);
}

}  // namespace uartex
}  // namespace esphome
