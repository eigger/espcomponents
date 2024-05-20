#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/valve/valve.h"

namespace esphome {
namespace uartex {

class UARTExValve : public valve::Valve, public UARTExDevice 
{
public:
    void dump_config() override;
    void setup() override;
    void loop() override;
    void set_state_position(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_position_func_ = f; }
    void set_state_open(state_t state) { this->state_open_ = state; }
    void set_state_closed(state_t state) { this->state_closed_ = state; }
    void set_command_open(cmd_t cmd) { this->command_open_ = cmd; }
    void set_command_close(cmd_t cmd) { this->command_close_ = cmd; }
    void set_command_stop(cmd_t cmd) { this->command_stop_ = cmd; }
protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(const valve::ValveCall &call) override;
    
protected:
    optional<std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_position_func_{};
    optional<state_t> state_open_{};
    optional<state_t> state_closed_{};
    optional<state_t> state_jammed_{};
    optional<state_t> state_locking_{};
    optional<state_t> state_unlocking_{};
    optional<cmd_t> command_open_{};
    optional<cmd_t> command_close_{};
    optional<cmd_t> command_stop_{};

};

}  // namespace uartex
}  // namespace esphome