#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace uartex {

class UARTExButton : public button::Button, public UARTExDevice 
{
public:
    void dump_config() override;

protected:
    void press_action() override { enqueue_tx_cmd(get_command_on()); }
};

}  // namespace uartex
}  // namespace esphome