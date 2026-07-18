#pragma once
#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeButton : public button::Button, public Component, public WsBridgeDevice {
 public:
  void setup() override {}
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void press_action() override {}
};

}  // namespace ws_bridge
}  // namespace esphome
