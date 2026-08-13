#pragma once
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeSelect : public select::Select, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing select instead of this platform
  // being the select itself. Commands from HA are applied to the wrapped
  // select, and its own state changes are what get reported back. The
  // options list inherits from it too unless this platform's own `options:`
  // is set (see ws_bridge_domains.h's ws_declare_select).
  void set_source(select::Select *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(const std::string &value) override;
  select::Select *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
