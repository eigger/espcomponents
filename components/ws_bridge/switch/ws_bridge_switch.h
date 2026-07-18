#pragma once
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeSwitch : public switch_::Switch, public Component, public WsBridgeDevice {
 public:
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void write_state(bool state) override;
};

}  // namespace ws_bridge
}  // namespace esphome
