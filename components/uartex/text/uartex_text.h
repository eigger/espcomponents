#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text/text.h"
#include "esphome/components/uartex/uartex_device.h"

namespace esphome {
namespace uartex {

class UARTExText : public text::Text, public UARTExDevice
{
public:
    void dump_config() override;
    void set_template(std::function<optional<std::string>(const uint8_t *data, const uint16_t len)> &&f) { this->state_text_func_ = f; }
    void set_command_text(std::function<cmd_t(const std::string &str)> f) { this->command_text_func_ = f; }
protected:
    void publish(const std::vector<uint8_t>& data) override;
    void control(const std::string &value) override;
    cmd_t* get_command_text();
protected:
    optional<std::function<optional<std::string>(const uint8_t *data, const uint16_t len)>> state_text_func_{};
    optional<std::function<cmd_t(const std::string &str)>> command_text_func_{};
    optional<cmd_t> command_text_{};
};

}  // namespace uartex
}  // namespace esphome
