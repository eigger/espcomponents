#pragma once
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "../ws_bridge_device.h"

#ifdef USE_DATETIME_DATE
#include "esphome/components/datetime/date_entity.h"
#endif
#ifdef USE_DATETIME_TIME
#include "esphome/components/datetime/time_entity.h"
#endif
#ifdef USE_DATETIME_DATETIME
#include "esphome/components/datetime/datetime_entity.h"
#endif

namespace esphome {
namespace ws_bridge {

// One class per `type:` — ESPHome's datetime component is three unrelated
// entity types behind one YAML domain, and each has its own base class, call
// object and state fields. HA sees them as the separate `date`, `time` and
// `datetime` platforms.
//
// Each is guarded by its own USE_DATETIME_* define: a config that only uses
// `type: DATE` never compiles the other two.

#ifdef USE_DATETIME_DATE
class WsBridgeDate : public datetime::DateEntity, public Component, public WsBridgeDevice {
 public:
  // Optional: mirror and drive an existing date. HA's set_value goes to the
  // wrapped entity; its state changes are what get reported back.
  void set_source(datetime::DateEntity *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(const datetime::DateCall &call) override;
  datetime::DateEntity *source_{nullptr};
};
#endif

#ifdef USE_DATETIME_TIME
class WsBridgeTime : public datetime::TimeEntity, public Component, public WsBridgeDevice {
 public:
  void set_source(datetime::TimeEntity *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(const datetime::TimeCall &call) override;
  datetime::TimeEntity *source_{nullptr};
};
#endif

#ifdef USE_DATETIME_DATETIME
class WsBridgeDateTime : public datetime::DateTimeEntity, public Component, public WsBridgeDevice {
 public:
  void set_source(datetime::DateTimeEntity *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  void control(const datetime::DateTimeCall &call) override;
  datetime::DateTimeEntity *source_{nullptr};
};
#endif

}  // namespace ws_bridge
}  // namespace esphome
