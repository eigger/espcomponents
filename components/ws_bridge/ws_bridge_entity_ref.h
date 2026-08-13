#pragma once
#include <string>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "ws_bridge_device.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_UPDATE
#include "esphome/components/update/update_entity.h"
#endif
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif
#ifdef USE_COVER
#include "esphome/components/cover/cover.h"
#endif
#ifdef USE_FAN
#include "esphome/components/fan/fan.h"
#endif

namespace esphome {
namespace ws_bridge {

// Bridges an *existing* ESPHome entity (given by `source_id:`) to Home
// Assistant over ws_bridge, configured from the hub's `entities:` list rather
// than as its own `platform: ws_bridge` entity — see the README's "Exposing
// existing entities" section. Declares straight off the source (device_class,
// unit_of_measurement, state_class, options, min/max/step, ...): there is no
// parallel entity here to hold YAML overrides for those, unlike the
// `platform: ws_bridge` + `*_id:` wrapping form, so `add_common_entity_fields`
// is always called with a null `ovr`.
//
// One concrete Ref class per domain (rather than a template) to match how
// every other WsBridgeDevice in this component is a plain, un-templated class.
class WsBridgeEntityRefBase : public Component, public WsBridgeDevice {
 public:
  // Empty means: use the source entity's own name, falling back to unique_id
  // if the source has no name of its own (id-only entity) — see ws_ha_name().
  void set_name_override(const std::string &name) { this->name_override_ = name; }

 protected:
  std::string name_override_;
};

#ifdef USE_SENSOR
class WsBridgeSensorRef : public WsBridgeEntityRefBase {
 public:
  void set_source(sensor::Sensor *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;

 protected:
  sensor::Sensor *source_{nullptr};
};
#endif

#ifdef USE_BINARY_SENSOR
class WsBridgeBinarySensorRef : public WsBridgeEntityRefBase {
 public:
  void set_source(binary_sensor::BinarySensor *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;

 protected:
  binary_sensor::BinarySensor *source_{nullptr};
};
#endif

#ifdef USE_TEXT_SENSOR
class WsBridgeTextSensorRef : public WsBridgeEntityRefBase {
 public:
  void set_source(text_sensor::TextSensor *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;

 protected:
  text_sensor::TextSensor *source_{nullptr};
};
#endif

#ifdef USE_SWITCH
class WsBridgeSwitchRef : public WsBridgeEntityRefBase {
 public:
  void set_source(switch_::Switch *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  switch_::Switch *source_{nullptr};
};
#endif

#ifdef USE_NUMBER
class WsBridgeNumberRef : public WsBridgeEntityRefBase {
 public:
  void set_source(number::Number *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  number::Number *source_{nullptr};
};
#endif

#ifdef USE_SELECT
class WsBridgeSelectRef : public WsBridgeEntityRefBase {
 public:
  void set_source(select::Select *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  select::Select *source_{nullptr};
};
#endif

#ifdef USE_BUTTON
class WsBridgeButtonRef : public WsBridgeEntityRefBase {
 public:
  void set_source(button::Button *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override {}
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  button::Button *source_{nullptr};
};
#endif

#ifdef USE_UPDATE
class WsBridgeUpdateRef : public WsBridgeEntityRefBase {
 public:
  void set_source(update::UpdateEntity *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  update::UpdateEntity *source_{nullptr};
};
#endif

#ifdef USE_LIGHT
class WsBridgeLightRef : public WsBridgeEntityRefBase, public light::LightRemoteValuesListener {
 public:
  void set_source(light::LightState *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;
  void on_light_remote_values_update() override;

 protected:
  light::LightState *source_{nullptr};
};
#endif

#ifdef USE_COVER
class WsBridgeCoverRef : public WsBridgeEntityRefBase {
 public:
  void set_source(cover::Cover *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  cover::Cover *source_{nullptr};
};
#endif

#ifdef USE_FAN
class WsBridgeFanRef : public WsBridgeEntityRefBase {
 public:
  void set_source(fan::Fan *source) { this->source_ = source; }
  const EntityBase *get_ws_bridge_source() const override { return this->source_; }
  void setup() override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  fan::Fan *source_{nullptr};
};
#endif

}  // namespace ws_bridge
}  // namespace esphome
