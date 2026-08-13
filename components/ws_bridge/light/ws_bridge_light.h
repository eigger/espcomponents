#pragma once
#include "esphome/components/light/light_state.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

// Bridges an existing ESPHome LightState to Home Assistant. Wrap-only (like
// update): YAML `id:` on a light is the LightState, and inheriting LightOutput
// would create a second LightState via new_light().
// Implements LightRemoteValuesListener — ESPHome 2026+ replaced the old
// std::function remote-values callback with this interface.
class WsBridgeLight : public Component,
                      public WsBridgeDevice,
                      public light::LightRemoteValuesListener {
 public:
  void set_light(light::LightState *light) { this->light_ = light; }
  const EntityBase *get_ws_bridge_source() const override { return this->light_; }
  void set_name(const std::string &name) { this->name_ = name; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;
  void on_light_remote_values_update() override;

 protected:
  std::string ha_name_() const;

  light::LightState *light_{nullptr};
  std::string name_;
};

}  // namespace ws_bridge
}  // namespace esphome
