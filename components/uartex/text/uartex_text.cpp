#include "uartex_text.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.text";

void UARTExText::dump_config() 
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    uartex_dump_config(TAG);
#endif
}

void UARTExText::publish(const std::vector<uint8_t>& data) 
{
    optional<std::string> val = get_state_str("lambda", data);
    if(val.has_value() && this->state != val.value())
    {
        this->state = val.value();
        publish_state(val.value());
    }
}

void UARTExText::control(const std::string& value)
{
    if (this->state == value) return;
    if (enqueue_tx_cmd(get_command_text(value)) || this->optimistic_)
    {
        this->state = value;
    }
    publish_state(this->state);
}

}  // namespace uartex
}  // namespace esphome
