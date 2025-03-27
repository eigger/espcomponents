#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/text/text.h"

namespace esphome {
namespace uartex {

class UARTExText : public text::Text, public UARTExDevice
{
public:
    void dump_config() override;
protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(const std::string& value) override;
    cmd_t* get_command_text(const std::string& str) { return get_command("command_text", str); }
protected:
};

}  // namespace uartex
}  // namespace esphome
