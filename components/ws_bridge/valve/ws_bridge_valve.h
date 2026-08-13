#pragma once
#include "esphome/components/valve/valve.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeValve : public valve::Valve, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing valve. HA commands go to the wrapped
  // valve; its state changes are what get reported back.
  void set_source(valve::Valve *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  valve::ValveTraits get_traits() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(const valve::ValveCall &call) override;
  valve::Valve *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
