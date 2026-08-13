#pragma once
#include <string>
#include "esphome/components/lock/lock.h"
#include "esphome/core/component.h"
#include "../ws_bridge_device.h"

namespace esphome {
namespace ws_bridge {

class WsBridgeLock : public lock::Lock, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing lock. HA commands go to the wrapped
  // lock; its state changes are what get reported back.
  void set_source(lock::Lock *source) { this->source_ = source; }
  // Declare-only regex for HA's code prompt — see CONF_CODE_FORMAT in const.py.
  void set_code_format(const std::string &code_format) { this->code_format_ = code_format; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(const lock::LockCall &call) override;
  void open_latch() override;
  lock::Lock *source_{nullptr};
  std::string code_format_;
};

}  // namespace ws_bridge
}  // namespace esphome
