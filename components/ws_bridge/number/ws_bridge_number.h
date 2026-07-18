#pragma once
#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeNumber : public number::Number, public Component, public WsBridgeDevice {
 public:
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(float value) override;
};

}  // namespace ws_bridge
}  // namespace esphome
