#include "uartex_number.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.number";

void UARTExNumber::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    log_config(TAG, "State Number", get_state_num("state_number"));
    log_config(TAG, "State Increment", get_state_increment());
    log_config(TAG, "State Decrement", get_state_decrement());
    log_config(TAG, "State ToMin", get_state_to_min());
    log_config(TAG, "State ToMax", get_state_to_max());
    uartex_dump_config(TAG);
#endif
}

void UARTExNumber::setup()
{
    float value = this->traits.get_min_value();;
    if (this->restore_value_)
    {
        this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
        this->pref_.load(&value);
    }
    this->state = value;
    publish_state(this->state);
}

void UARTExNumber::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    float min = this->traits.get_min_value();
    float max = this->traits.get_max_value();
    float step = this->traits.get_step();
    if (verify_state(data, get_state_increment()))
    {
        this->state += step;
        changed = true;
    }
    else if (verify_state(data, get_state_decrement()))
    {
        this->state -= step;
        changed = true;
    }
    else if (verify_state(data, get_state_to_min()))
    {
        this->state = min;
        changed = true;
    }
    else if (verify_state(data, get_state_to_max()))
    {
        this->state = max;
        changed = true;
    }
    optional<float> val = get_state_number(data);
    if (val.has_value() && this->state != val.value())
    {
        this->state = val.value();
        changed = true;
    }
    if (this->state > max) this->state = max;
    if (this->state < min) this->state = min;
    if (changed)
    {
        if (this->restore_value_) this->pref_.save(&this->state);
        publish_state(this->state);
    }
}

void UARTExNumber::control(float value)
{
    if (this->state == value) return;
    if (enqueue_tx_cmd(get_command_number(value)) || this->optimistic_)
    {
        this->state = value;
        if (this->restore_value_) this->pref_.save(&value);
    }    
    publish_state(this->state);
}

}  // namespace uartex
}  // namespace esphome
