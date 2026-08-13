#pragma once
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeSwitch : public switch_::Switch, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing switch instead of this platform
  // being the switch itself. Commands from HA are applied to the wrapped
  // switch (turn_on()/turn_off() on it, not on this), and its own state
  // changes are what get reported back — see ws_bridge_domains.h.
  void set_source(switch_::Switch *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void write_state(bool state) override;
  switch_::Switch *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
