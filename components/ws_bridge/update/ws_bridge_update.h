#pragma once
#include "esphome/components/update/update_entity.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

// Bridges an existing ESPHome UpdateEntity (http_request OTA) to Home Assistant
// as an `update` entity over the ws_bridge protocol. Does not perform the
// download/flash itself — that stays with the wrapped http_request update.
class WsBridgeUpdate : public Component, public WsBridgeDevice {
 public:
  void set_update(update::UpdateEntity *update) { this->update_ = update; }
  const EntityBase *get_ws_bridge_source() const override { return this->update_; }
  void set_name(const std::string &name) { this->name_ = name; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  std::string ha_name_() const;

  update::UpdateEntity *update_{nullptr};
  std::string name_;
};

}  // namespace ws_bridge
}  // namespace esphome
