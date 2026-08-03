#pragma once
#include "esphome/core/automation.h"
#include "ws_bridge.h"

namespace esphome {
namespace ws_bridge {

class ConnectedTrigger : public Trigger<> {
 public:
  explicit ConnectedTrigger(WsBridgeComponent *parent) {
    parent->add_on_connected_callback([this]() { this->trigger(); });
  }
};

class DisconnectedTrigger : public Trigger<> {
 public:
  explicit DisconnectedTrigger(WsBridgeComponent *parent) {
    parent->add_on_disconnected_callback([this]() { this->trigger(); });
  }
};

// Fires whenever entity declarations are (re)sent: on connect and on every
// periodic re-announce. Hook manual send_entity_declare() calls here.
class DeclareTrigger : public Trigger<> {
 public:
  explicit DeclareTrigger(WsBridgeComponent *parent) {
    parent->add_on_declare_callback([this]() { this->trigger(); });
  }
};

}  // namespace ws_bridge
}  // namespace esphome
