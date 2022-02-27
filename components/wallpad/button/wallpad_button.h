#pragma once

#include "esphome/components/wallpad/wallpad_device.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace wallpad {

class WallPadButton : public button::Button, public WallPadDevice 
{
  public:
        WallPadButton() { device_name_ = &this->name_; }
        void dump_config() override;
        void publish(const uint8_t *data, const num_t len) override;
        bool publish(bool state) override { return true; }
        void press_action() override { push_command(this->get_command_on()); }

  protected:

};

}  // namespace wallpad
}  // namespace esphome