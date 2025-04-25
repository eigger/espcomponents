#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace uartex {

class UARTExCover : public cover::Cover, public UARTExDevice 
{
public:
    void dump_config() override;
    void setup() override;

protected:
    cover::CoverTraits get_traits() override;
    void publish(const std::vector<uint8_t>& data) override;
    void control(const cover::CoverCall &call) override;
    state_t* get_state_open() { return get_state("state_open"); }
    state_t* get_state_closed() { return get_state("state_closed"); }
    cmd_t* get_command_open() { return get_command("command_open"); }
    cmd_t* get_command_close() { return get_command("command_close"); }
    cmd_t* get_command_stop() { return get_command("command_stop"); }
    cmd_t* get_command_position(const float x) { return get_command("command_position", x); }
    cmd_t* get_command_tilt(const float x) { return get_command("command_tilt", x); }
    optional<float> get_state_position(const std::vector<uint8_t>& data) { return get_state_float("state_position", data); }
    optional<float> get_state_tilt(const std::vector<uint8_t>& data) { return get_state_float("state_tilt", data); }
    bool has_state_position() { return has_state("state_position"); } 
    bool has_state_tilt() { return has_state("state_tilt"); } 
protected:

};

}  // namespace uartex
}  // namespace esphome