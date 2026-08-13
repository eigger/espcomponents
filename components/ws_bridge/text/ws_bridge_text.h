#pragma once
#include "esphome/components/text/text.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeText : public text::Text, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing text input. HA's set_value goes to
  // the wrapped text; its state changes are what get reported back, and its
  // length limits / pattern / mode are what get declared.
  void set_source(text::Text *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(const std::string &value) override;
  text::Text *source_{nullptr};
};

}  // namespace ws_bridge
}  // namespace esphome
