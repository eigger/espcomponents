#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/select/select.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace uartex {

class UARTExSelect : public select::Select, public UARTExDevice
{
public:
    void dump_config() override;
    void setup() override;
    void set_initial_option(const std::string &initial_option) { this->initial_option_ = initial_option; }
    void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }
protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(const std::string &value) override;
    cmd_t* get_command_select(const std::string& str) { return get_command("command_select", str); }
    optional<std::string> get_state_select(const std::vector<uint8_t>& data) { return get_state_str("state_select", data); }
protected:
    ESPPreferenceObject pref_;
    std::string initial_option_;
    bool restore_value_ = false;
};

}  // namespace uartex
}  // namespace esphome
