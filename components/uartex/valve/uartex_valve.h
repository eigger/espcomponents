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

protected:
    valve::ValveTraits get_traits() override;
    void publish(const std::vector<uint8_t>& data) override;
    void control(const valve::ValveCall& call) override;
    state_t* get_state_open() { return get_state("state_open"); }
    state_t* get_state_closed() { return get_state("state_closed"); }
    cmd_t* get_command_open() { return get_command("command_open"); }
    cmd_t* get_command_close() { return get_command("command_close"); }
    cmd_t* get_command_stop() { return get_command("command_stop"); }
    optional<float> get_state_position(const std::vector<uint8_t>& data) { return get_state_float("state_position", data); }
    bool has_state_position() { return has_state("state_position"); } 
protected:

};

}  // namespace uartex
}  // namespace esphome