#pragma once
#include <string>
#include "ws_protocol.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeComponent;

// Shared base for every ws_bridge platform entity (sensor/binary_sensor/switch/
// number/select/button/update). Holds the protocol-facing identity fields and gives
// the hub a uniform way to ask any registered entity to (re)send its
// ws_bridge/entity declaration (used on first connect and on every reconnect).
class WsBridgeDevice {
 public:
  void set_ws_bridge_parent(WsBridgeComponent *parent) { this->parent_ = parent; }
  void set_unique_id(const std::string &id) { this->unique_id_ = id; }
  void set_device_id(const std::string &id) { this->device_id_ = id; }
  void set_device_name(const std::string &name) { this->device_name_ = name; }

  // Read side of the identity fields above. These exist so the per-domain
  // helpers in ws_bridge_domains.h can be plain free functions taking a
  // WsBridgeDevice — the same helper then serves both a `platform: ws_bridge`
  // entity (which is its own source) and a wrapper pointing at a foreign
  // entity, without either duplicating the declare/state/command logic.
  const std::string &get_ws_bridge_unique_id() const { return this->unique_id_; }
  const std::string &get_ws_bridge_device_id() const { return this->device_id_; }
  const std::string &get_ws_bridge_device_name() const { return this->device_name_; }
  WsBridgeComponent *get_ws_bridge_parent() const { return this->parent_; }

  // Ask this entity to send its ws_bridge/entity declaration (and, if it has
  // one, its current state) over the parent connection. Called by the hub
  // once per entity on (re)connect.
  virtual void ws_bridge_declare() = 0;

  // Called by the hub when a command arrives for this entity's unique_id.
  // Read-only platforms (sensor/binary_sensor) never receive commands and
  // keep the no-op default.
  virtual void ws_bridge_handle_command(const WsCommand &command) {}

 protected:
  WsBridgeComponent *parent_{nullptr};
  std::string unique_id_;
  std::string device_id_;
  std::string device_name_;
};

}  // namespace ws_bridge
}  // namespace esphome
