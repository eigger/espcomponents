#include "uartex_lock.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.lock";

void UARTExLock::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    log_config(TAG, "State Locked", get_state_locked());
    log_config(TAG, "State Unlocked", get_state_unlocked());
    log_config(TAG, "State Jammed", get_state_jammed());
    log_config(TAG, "State Locking", get_state_locking());
    log_config(TAG, "State Unlocking", get_state_unlocking());
    log_config(TAG, "Command Lock", get_command_lock());
    log_config(TAG, "Command Unlock", get_command_unlock());
    uartex_dump_config(TAG);
#endif
}

void UARTExLock::setup()
{
    if (get_state_locked() || get_command_lock()) traits.add_supported_state(lock::LOCK_STATE_LOCKED);
    if (get_state_unlocked() || get_command_unlock()) traits.add_supported_state(lock::LOCK_STATE_UNLOCKED);
    if (get_state_jammed()) traits.add_supported_state(lock::LOCK_STATE_JAMMED);
    if (get_state_locked() || get_state_locking()) traits.add_supported_state(lock::LOCK_STATE_LOCKING);
    if (get_state_unlocked() || get_state_unlocking()) traits.add_supported_state(lock::LOCK_STATE_UNLOCKING);
}

void UARTExLock::loop()
{
    if (!get_state_locking())
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
    if (!get_state_unlocking())
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
    if (verify_state(data, get_state_locked()))
    {
        this->state = lock::LOCK_STATE_LOCKED;
        changed = true;
    }
    else if (verify_state(data, get_state_unlocked()))
    {
        this->state = lock::LOCK_STATE_UNLOCKED;
        changed = true;
    }
    else if (verify_state(data, get_state_jammed()))
    {
        this->state = lock::LOCK_STATE_JAMMED;
        changed = true;
    }
    else if (verify_state(data, get_state_locking()))
    {
        this->state = lock::LOCK_STATE_LOCKING;
        changed = true;
    }
    else if (verify_state(data, get_state_unlocking()))
    {
        this->state = lock::LOCK_STATE_UNLOCKING;
        changed = true;
    }
    if (changed) publish_state(this->state);
}

void UARTExLock::control(const lock::LockCall& call)
{
    if (this->state != *call.get_state())
    {

        this->timer_ = get_time();
        switch (*call.get_state())
        {
        case lock::LOCK_STATE_LOCKED:
            if (enqueue_tx_cmd(get_command_lock()) || this->optimistic_)
            {
                this->state = *call.get_state();
                if (get_state_locked()) this->state = lock::LOCK_STATE_LOCKING;
            }
            break;
        case lock::LOCK_STATE_UNLOCKED:
            if (enqueue_tx_cmd(get_command_unlock()) || this->optimistic_)
            {
                this->state = *call.get_state();
                if (get_state_unlocked()) this->state = lock::LOCK_STATE_UNLOCKING;
            }
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
