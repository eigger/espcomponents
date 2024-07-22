#include "uartex_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.number";

void UARTExNumber::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Number '%s':", get_name().c_str());
    uartex_dump_config(TAG);
}

void UARTExNumber::setup()
{
    this->state = this->traits.get_min_value();
    publish_state(this->state);
}

void UARTExNumber::publish(const std::vector<uint8_t>& data)
{
    optional<float> val = get_state_float("state_number", data);
    if (val.has_value() && this->state != val.value())
    {
        this->state = val.value();
        publish_state(this->state);
    }
}

void UARTExNumber::control(float value)
{
    if (this->state == value) return;
    this->state = value;
    enqueue_tx_cmd(get_command_number(this->state));
    publish_state(value);
}

}  // namespace uartex
}  // namespace esphome
