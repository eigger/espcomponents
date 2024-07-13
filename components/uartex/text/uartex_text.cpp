#include "uartex_text.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.text";

void UARTExText::dump_config() 
{
    ESP_LOGCONFIG(TAG, "UARTEx Text '%s':", get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExText::publish(const std::vector<uint8_t>& data) 
{
    if (this->state_text_func_.has_value())
    {
        optional<std::string> val = (*this->state_text_func_)(&data[0], data.size());
        if(val.has_value() && this->state != val.value())
        {
            this->state = val.value();
            publish_state(val.value());
        }
    }
}

void UARTExText::control(const std::string &value)
{
    if (this->state == value) return;
    this->state = value;
    enqueue_tx_cmd(get_command_text());
    publish_state(value);
}

cmd_t *UARTExText::get_command_text()
{
    if (this->command_text_func_.has_value())
        this->command_text_ = (*this->command_text_func_)(this->state);
    return &this->command_text_.value();
}

}  // namespace uartex
}  // namespace esphome
