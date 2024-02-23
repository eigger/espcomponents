#include "uartex_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.number";

void UARTExNumber::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Number '%s':", get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExNumber::setup()
{
    this->state = this->traits.get_min_value();
    publish_state(this->state);
}

void UARTExNumber::publish(const std::vector<uint8_t>& data)
{
    if (this->state_number_func_.has_value())
    {
        optional<float> val = (*this->state_number_func_)(&data[0], data.size(), this->state);
        if (val.has_value() && this->state != val.value())
        {
            this->state = val.value();
            publish_state(this->state);
        }
    }
    else if (this->state_number_.has_value() && data.size() >= (this->state_number_.value().offset + this->state_number_.value().length))
    {
        float val = state_to_float(data, this->state_number_.value());
        if (this->state != val)
        {
            this->state = val;
            publish_state(this->state);
        }
    }
}

void UARTExNumber::control(float value)
{
    if (this->state == value) return;
    this->state = value;
    if (this->command_number_func_.has_value()) enqueue_tx_cmd(get_command_number());
    publish_state(value);
}

cmd_t *UARTExNumber::get_command_number()
{
    if (this->command_number_func_.has_value())
        this->command_number_ = (*this->command_number_func_)(this->state);
    return &this->command_number_.value();
}

}  // namespace uartex
}  // namespace esphome
