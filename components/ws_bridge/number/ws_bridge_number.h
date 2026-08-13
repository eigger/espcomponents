#pragma once
#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeNumber : public number::Number, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing number instead of this platform
  // being the number itself. Commands from HA are applied to the wrapped
  // number, and its own state changes are what get reported back. min/max/step
  // inherit from it per field, each one unless overridden here (see
  // ws_bridge_domains.h's ws_declare_number).
  void set_source(number::Number *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(float value) override;
  number::Number *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
