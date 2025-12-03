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
    ESP_LOGD(TAG, "Setting up");
    size_t index = this->initial_option_index_;
    if (this->restore_value_)
    {
        this->pref_ = global_preferences->make_preference<size_t>(this->get_preference_hash());
        size_t restored_index;
        if (this->pref_.load(&restored_index) && this->has_index(restored_index))
        {
            index = restored_index;
            ESP_LOGD(TAG, "State from restore: %s", this->option_at(index));
        } 
        else 
        {
            ESP_LOGD(TAG, "State from initial (could not load or invalid stored index): %s", this->option_at(index));
        }
    } 
    else
    {
        ESP_LOGD(TAG, "State from initial: %s", this->option_at(index));
    }
    this->publish_state(index);
}

void UARTExSelect::publish(const std::vector<uint8_t>& data) 
{
    optional<std::string> val = get_state_select(data);
    if(val.has_value() && this->current_option() != val.value())
    {
        auto idx = this->index_of(val.value());
        if (idx.has_value())
        {
            this->publish_state(idx.value());
        }
    }
}

void UARTExSelect::control(size_t index)
{
    if (this->active_index().has_value() && active_index().value() == index) return;
    optional<std::string> val = this->at(index);
    if (enqueue_tx_cmd(get_command_select(val.has_value() ? val.value() : "")) || this->optimistic_)
    {
        this->publish_state(index);
    }
    if (this->restore_value_) this->pref_.save(&index);
}

}  // namespace uartex
}  // namespace esphome
