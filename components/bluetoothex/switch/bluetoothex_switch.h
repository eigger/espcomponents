#pragma once

#include "esphome/components/bluetoothex/bluetoothex_device.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace bluetoothex {

class BluetoothExSwitch : public switch_::Switch, public BluetoothExDevice 
{
public:
    BluetoothExSwitch() { device_name_ = &this->name_; }
    void dump_config() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override { publish_state(state); return true; }

    void write_state(bool state) override 
    {
        if(state == this->state) return;
        push_tx_cmd(state ? this->get_command_on() : this->get_command_off());
        publish_state(state);
    }

protected:

};

}  // namespace bluetoothex
}  // namespace esphome