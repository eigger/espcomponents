#pragma once

#include "esphome/components/rs485/rs485.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace rs485 {

class RS485Switch : public switch_::Switch, public RS485Device {
  public:
        RS485Switch() { device_name_ = &this->name_; }
        void dump_config() override;
        void publish(const uint8_t *data, const num_t len) override;
        bool publish(bool state) override { publish_state(state); return true; }

        void write_state(bool state) override {
            if(state == this->state) return;
            
            write_with_header(state ? this->get_command_on() : this->get_command_off());
            publish_state(state);
        }

  protected:

};

}  // namespace rs485
}  // namespace esphome