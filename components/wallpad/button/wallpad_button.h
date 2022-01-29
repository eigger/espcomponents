#pragma once

#include "esphome/components/wallpad/wallpad.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace wallpad {

class WallPadButton : public button_::Button, public WallPadDevice 
{
  public:
        WallPadButton() { device_name_ = &this->name_; }
        void dump_config() override;
        // void publish(const uint8_t *data, const num_t len) override;
        // bool publish(bool state) override { publish_state(state); return true; }
        void press_action() override
        {
            write_with_header(this->get_command_on());
        }

  protected:

};

}  // namespace wallpad
}  // namespace esphome