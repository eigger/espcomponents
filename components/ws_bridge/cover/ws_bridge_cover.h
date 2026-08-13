#pragma once
#include "esphome/components/cover/cover.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeCover : public cover::Cover, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing cover. HA commands go to the wrapped
  // cover; its state changes are what get reported back.
  void set_source(cover::Cover *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  cover::CoverTraits get_traits() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(const cover::CoverCall &call) override;
  cover::Cover *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
