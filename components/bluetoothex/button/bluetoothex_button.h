#pragma once

#include "esphome/components/bluetoothex/bluetoothex_device.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace bluetoothex {

class BluetoothExButton : public button::Button, public BluetoothExDevice 
{
public:
    BluetoothExButton() { device_name_ = &this->name_; }
    void dump_config() override;
    void publish(const std::vector<uint8_t>& data) override;
    bool publish(bool state) override { return true; }
    void press_action() override { push_tx_cmd(this->get_command_on()); }

protected:
};

}  // namespace bluetoothex
}  // namespace esphome