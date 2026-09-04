#pragma once

#include "esphome/core/automation.h"
#include "ble_elm327.h"

namespace esphome {
namespace ble_elm327 {

template<typename... Ts> class BleElm327SendCommandAction : public Action<Ts...> {
 public:
  explicit BleElm327SendCommandAction(BleElm327Component *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, command)

  void play(const Ts &...x) override {
    this->parent_->send_command(this->command_.value(x...));
  }

 protected:
  BleElm327Component *parent_;
};

}  // namespace ble_elm327
}  // namespace esphome
