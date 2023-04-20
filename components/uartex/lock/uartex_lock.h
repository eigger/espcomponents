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
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override { return false; }
    void control(const lock::LockCall &call) override;
    void set_state_locked(state_t state) { state_locked_ = state; }
    void set_state_unlocked(state_t state) { state_unlocked_ = state; }
    void set_state_jammed(state_t state) { state_jammed_ = state; }
    void set_state_locking(state_t state) { state_locking_ = state; }
    void set_state_unlocking(state_t state) { state_unlocking_ = state; }
    void set_command_lock(cmd_t cmd) { command_lock_ = cmd; }
    void set_command_unlock(cmd_t cmd) { command_unlock_ = cmd; }
protected:
    optional<state_t> state_locked_{};
    optional<state_t> state_unlocked_{};
    optional<state_t> state_jammed_{};
    optional<state_t> state_locking_{};
    optional<state_t> state_unlocking_{};

    optional<cmd_t> command_lock_{};
    optional<cmd_t> command_unlock_{};

};

}  // namespace uartex
}  // namespace esphome