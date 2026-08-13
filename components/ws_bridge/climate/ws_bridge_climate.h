#pragma once
#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeClimate : public climate::Climate, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing climate. HA commands go to the wrapped
  // climate; its state changes are what get reported back.
  void set_source(climate::Climate *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  // Climate exposes public non-virtual get_traits(); integrations override traits().
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  climate::Climate *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
