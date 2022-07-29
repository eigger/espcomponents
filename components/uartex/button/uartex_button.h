#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace uartex {

class UARTExButton : public button::Button, public UARTExDevice 
{
  public:
        UARTExButton() { device_name_ = &this->name_; }
        void dump_config() override;
        void publish(const std::vector<uint8_t>& data) override;
        bool publish(bool state) override { return true; }
        void press_action() override { push_command(this->get_command_on()); }

  protected:

};

}  // namespace uartex
}  // namespace esphome