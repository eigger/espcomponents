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
    void set_state_locked(state_t state) { this->state_map_["state_locked"] = state; }
    void set_state_unlocked(state_t state) { this->state_map_["state_unlocked"] = state;}
    void set_state_jammed(state_t state) { this->state_map_["state_jammed"] = state; }
    void set_state_locking(state_t state) { this->state_map_["state_locking"] = state; }
    void set_state_unlocking(state_t state) { this->state_map_["state_unlocking"] = state; }
    void set_command_lock(cmd_t cmd) { this->command_map_["command_lock"] = cmd; }
    void set_command_lock(std::function<cmd_t()> f) { this->command_func_map_["command_lock"] = f; }
    void set_command_unlock(cmd_t cmd) { this->command_map_["command_unlock"] = cmd; }
    void set_command_unlock(std::function<cmd_t()> f) { this->command_func_map_["command_unlock"] = f; }
    void set_lock_timeout(uint16_t timeout) { this->conf_lock_timeout_ = timeout; }
    void set_unlock_timeout(uint16_t timeout) { this->conf_unlock_timeout_ = timeout; }


    state_t* get_state_locked() { return get_state("state_locked"); }
    state_t* get_state_unlocked() { return get_state("state_unlocked"); }
    state_t* get_state_jammed() { return get_state("state_jammed"); }
    state_t* get_state_locking() { return get_state("state_locking"); }
    state_t* get_state_unlocking() { return get_state("state_unlocking"); }
    cmd_t* get_command_lock() { return get_command("command_lock"); }
    cmd_t* get_command_unlock() { return get_command("command_unlock"); }
protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(const lock::LockCall &call) override;
    
protected:
    uint16_t conf_lock_timeout_{5 * 1000};
    uint16_t conf_unlock_timeout_{5 * 1000};
    unsigned long timer_{0};

};

}  // namespace uartex
}  // namespace esphome