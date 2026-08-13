#pragma once
#include "esphome/components/event/event.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

// Read-only: HA never sends commands to an event entity, so there is no
// ws_bridge_handle_command() override here.
class WsBridgeEvent : public event::Event, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror an existing event. Its triggers are what get forwarded to
  // HA, and its event_types are what get declared.
  void set_source(event::Event *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;

 protected:
  event::Event *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
