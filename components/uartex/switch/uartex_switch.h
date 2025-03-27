#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace uartex {

class UARTExSwitch : public switch_::Switch, public UARTExDevice 
{
public:
    void dump_config() override;

protected:
    void publish(const bool state) override { publish_state(state); }
    void write_state(bool state) override;
};

}  // namespace uartex
}  // namespace esphome