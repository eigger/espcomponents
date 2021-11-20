#pragma once

#include "esphome/components/uartex/uartex.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace uartex {

class UartExSwitch : public switch_::Switch, public UartExDevice {
  public:
        UartExSwitch() { device_name_ = &this->name_; }
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

}  // namespace uartex
}  // namespace esphome