#include "uartex_lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.lock";

void UARTExLock::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Lock '%s':", get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExLock::setup()
{
    if (this->state_locked_.has_value() || this->command_lock_.has_value()) traits.add_supported_state(lock::LOCK_STATE_LOCKED);
    if (this->state_unlocked_.has_value() || this->command_unlock_.has_value()) traits.add_supported_state(lock::LOCK_STATE_UNLOCKED);
    if (this->state_jammed_.has_value()) traits.add_supported_state(lock::LOCK_STATE_JAMMED);
    if (this->state_locked_.has_value() || this->state_locking_.has_value()) traits.add_supported_state(lock::LOCK_STATE_LOCKING);
    if (this->state_unlocked_.has_value() || this->state_unlocking_.has_value()) traits.add_supported_state(lock::LOCK_STATE_UNLOCKING);
}

void UARTExLock::loop()
{
    if (!this->state_locking_.has_value())
    {
        if (this->state == lock::LOCK_STATE_LOCKING)
        {
            if (elapsed_time(this->timer_) > conf_lock_timeout_)
            {
                this->state = lock::LOCK_STATE_JAMMED;
                publish_state(this->state);
            }
        }
    }
    if (!this->state_unlocking_.has_value())
    {
        if (this->state == lock::LOCK_STATE_UNLOCKING)
        {
            if (elapsed_time(this->timer_) > conf_unlock_timeout_)
            {
                this->state = lock::LOCK_STATE_JAMMED;
                publish_state(this->state);
            }
        }
    }
}

void UARTExLock::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    if (this->state_locked_.has_value() && verify_state(data, &this->state_locked_.value()))
    {
        this->state = lock::LOCK_STATE_LOCKED;
        changed = true;
    }
    else if (this->state_unlocked_.has_value() && verify_state(data, &this->state_unlocked_.value()))
    {
        this->state = lock::LOCK_STATE_UNLOCKED;
        changed = true;
    }
    else if (this->state_jammed_.has_value() && verify_state(data, &this->state_jammed_.value()))
    {
        this->state = lock::LOCK_STATE_JAMMED;
        changed = true;
    }
    else if (this->state_locking_.has_value() && verify_state(data, &this->state_locking_.value()))
    {
        this->state = lock::LOCK_STATE_LOCKING;
        changed = true;
    }
    else if (this->state_unlocking_.has_value() && verify_state(data, &this->state_unlocking_.value()))
    {
        this->state = lock::LOCK_STATE_UNLOCKING;
        changed = true;
    }
    if (changed) publish_state(this->state);
}

void UARTExLock::control(const lock::LockCall &call)
{
    if (this->state != *call.get_state())
    {
        this->state = *call.get_state();
        this->timer_ = get_time();
        switch (this->state)
        {
        case lock::LOCK_STATE_LOCKED:
            if (this->command_lock_.has_value()) enqueue_tx_cmd(&this->command_lock_.value());
            if (this->state_locked_.has_value()) this->state = lock::LOCK_STATE_LOCKING;
            break;
        case lock::LOCK_STATE_UNLOCKED:
            if (this->command_unlock_.has_value()) enqueue_tx_cmd(&this->command_unlock_.value());
            if (this->state_unlocked_.has_value()) this->state = lock::LOCK_STATE_UNLOCKING;
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
        publish_state(this->state);
    }
}

}  // namespace uartex
}  // namespace esphome
