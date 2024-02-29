#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/lock/lock.h"

namespace esphome {
namespace uartex {

class UARTExLock : public lock::Lock, public UARTExDevice 
{
public:
    void dump_config() override;
    void setup() override;
    void loop() override;
    void set_state_locked(state_t state) { this->state_locked_ = state; }
    void set_state_unlocked(state_t state) { this->state_unlocked_ = state; }
    void set_state_jammed(state_t state) { this->state_jammed_ = state; }
    void set_state_locking(state_t state) { this->state_locking_ = state; }
    void set_state_unlocking(state_t state) { this->state_unlocking_ = state; }
    void set_command_lock(cmd_t cmd) { this->command_lock_ = cmd; }
    void set_command_unlock(cmd_t cmd) { this->command_unlock_ = cmd; }
    void set_lock_timeout(uint16_t timeout) { this->conf_lock_timeout_ = timeout; }
    void set_unlock_timeout(uint16_t timeout) { this->conf_unlock_timeout_ = timeout; }

protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(const lock::LockCall &call) override;
    
protected:
    optional<state_t> state_locked_{};
    optional<state_t> state_unlocked_{};
    optional<state_t> state_jammed_{};
    optional<state_t> state_locking_{};
    optional<state_t> state_unlocking_{};
    optional<cmd_t> command_lock_{};
    optional<cmd_t> command_unlock_{};
    uint16_t conf_lock_timeout_{5 * 1000};
    uint16_t conf_unlock_timeout_{5 * 1000};
    unsigned long timer_{0};

};

}  // namespace uartex
}  // namespace esphome