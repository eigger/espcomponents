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
    void publish(bool state) override {
        this->has_published_state_ = true;
        this->publish_state(state);
    }
    void write_state(bool state) override;
    bool has_published_state_{false};
};

}  // namespace uartex
}  // namespace esphome