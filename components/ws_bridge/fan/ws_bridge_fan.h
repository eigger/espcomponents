#pragma once
#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeFan : public fan::Fan, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing fan. HA commands go to the wrapped
  // fan; its state changes are what get reported back.
  void set_source(fan::Fan *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  fan::FanTraits get_traits() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(const fan::FanCall &call) override;
  fan::Fan *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
