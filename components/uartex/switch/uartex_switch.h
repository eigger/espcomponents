#pragma once

#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace uartex {

class UARTExSwitch : public switch_::Switch, public UARTExDevice 
{
public:
    void dump_config() override;
    bool publish(bool state) override { publish_state(state); return true; }

    void write_state(bool state) override 
    {
        if(state == this->state) return;
        enqueue_tx_cmd(state ? get_command_on() : get_command_off());
        publish_state(state);
    }

protected:

};

}  // namespace uartex
}  // namespace esphome