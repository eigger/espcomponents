#include "uartex_select.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.select";

void UARTExSelect::dump_config() 
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    uartex_dump_config(TAG);
#endif
}

void UARTExSelect::setup()
{
    std::string value;
    ESP_LOGD(TAG, "Setting up");
    if (!this->restore_value_)
    {
        value = this->initial_option_;
        ESP_LOGD(TAG, "State from initial: %s", value.c_str());
    } 
    else 
    {
        size_t index;
        this->pref_ = global_preferences->make_preference<size_t>(this->get_object_id_hash());
        if (!this->pref_.load(&index)) 
        {
            value = this->initial_option_;
            ESP_LOGD(TAG, "State from initial (could not load stored index): %s", value.c_str());
        } 
        else if (!this->has_index(index)) 
        {
            value = this->initial_option_;
            ESP_LOGD(TAG, "State from initial (restored index %d out of bounds): %s", index, value.c_str());
        } 
        else 
        {
            value = this->at(index).value();
            ESP_LOGD(TAG, "State from restore: %s", value.c_str());
        }
    }
    this->publish_state(value);
}

void UARTExSelect::publish(const std::vector<uint8_t>& data) 
{
    optional<std::string> val = get_state_select(data);
    if(val.has_value() && this->state != val.value())
    {
        if (this->has_option(val.value()))
        {
            this->state = val.value();
            this->publish_state(val.value());
        }
    }
}

void UARTExSelect::control(const std::string& value)
{
    if (this->state == value) return;
    if (enqueue_tx_cmd(get_command_select(value)) || this->optimistic_)
    {
        this->state = value;
    }
    this->publish_state(this->state);
    if (this->restore_value_)
    {
        auto index = this->index_of(value);
        this->pref_.save(&index.value());
    }
}

}  // namespace uartex
}  // namespace esphome
