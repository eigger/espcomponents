#include "uartex_lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.lock";

void UARTExLock::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Lock '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

void UARTExLock::setup()
{
    //if (this->command_unlock_.has_value()) traits.set_supports_open(true);
    if (this->state_locked_.has_value()) traits.add_supported_state(lock::LOCK_STATE_LOCKED);
    if (this->state_unlocked_.has_value()) traits.add_supported_state(lock::LOCK_STATE_UNLOCKED);
    if (this->state_jammed_.has_value()) traits.add_supported_state(lock::LOCK_STATE_JAMMED);
    if (this->state_locking_.has_value()) traits.add_supported_state(lock::LOCK_STATE_LOCKING);
    if (this->state_unlocking_.has_value()) traits.add_supported_state(lock::LOCK_STATE_UNLOCKING);
}

void UARTExLock::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    if (state_locked_.has_value() && validate(data, &state_locked_.value()))
    {
        state = lock::LOCK_STATE_LOCKED;
        changed = true;
    }
    else if (state_unlocked_.has_value() && validate(data, &state_unlocked_.value()))
    {
        state = lock::LOCK_STATE_UNLOCKED;
        changed = true;
    }
    else if (state_jammed_.has_value() && validate(data, &state_jammed_.value()))
    {
        state = lock::LOCK_STATE_JAMMED;
        changed = true;
    }
    else if (state_locking_.has_value() && validate(data, &state_locking_.value()))
    {
        state = lock::LOCK_STATE_LOCKING;
        changed = true;
    }
    else if (state_unlocking_.has_value() && validate(data, &state_unlocking_.value()))
    {
        state = lock::LOCK_STATE_UNLOCKING;
        changed = true;
    }
    if (changed) this->publish_state(state);
}

void UARTExLock::control(const lock::LockCall &call)
{
    if (this->state != *call.get_state())
    {
        this->state = *call.get_state();
        switch (this->state)
        {
        case lock::LOCK_STATE_LOCKED:
            if (this->command_lock_.has_value()) push_tx_cmd(&this->command_lock_.value());
            else this->state = lock::LOCK_STATE_UNLOCKED;
            break;
        case lock::LOCK_STATE_UNLOCKED:
            if (this->command_unlock_.has_value()) push_tx_cmd(&this->command_unlock_.value());
            else this->state = lock::LOCK_STATE_LOCKED;
            break;
        case lock::LOCK_STATE_LOCKING:
            break;        
        case lock::LOCK_STATE_UNLOCKING:
            break;
        case lock::LOCK_STATE_NONE:
            break;
        case lock::LOCK_STATE_JAMMED:
            break;
        }
        this->publish_state(this->state);
    }
}

}  // namespace uartex
}  // namespace esphome
