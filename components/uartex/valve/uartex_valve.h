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
    void set_state_position(std::function<optional<float>(const uint8_t *data, const uint16_t len)> f) { this->state_func_map_["state_position"] = f; }
    void set_state_open(state_t state) { this->state_map_["state_open"] = state;}
    void set_state_closed(state_t state) { this->state_map_["state_closed"] = state;}
    void set_command_open(cmd_t cmd) { this->command_map_["command_open"] = cmd; }
    void set_command_close(cmd_t cmd) { this->command_map_["command_close"] = cmd; }
    void set_command_stop(cmd_t cmd) { this->command_map_["command_stop"] = cmd; }
protected:
    valve::ValveTraits get_traits() override;
    void publish(const std::vector<uint8_t>& data) override;
    void control(const valve::ValveCall &call) override;
    
    state_t* get_state_open() { return get_state("state_open"); }
    state_t* get_state_closed() { return get_state("state_closed"); }
    cmd_t* get_command_open() { return get_command("command_open"); }
    cmd_t* get_command_close() { return get_command("command_close"); }
    cmd_t* get_command_stop() { return get_command("command_stop"); }
protected:

};

}  // namespace uartex
}  // namespace esphome