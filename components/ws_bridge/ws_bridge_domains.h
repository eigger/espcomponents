#pragma once
#include <cmath>
#include <string>
#include "esphome/core/defines.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/log.h"
#include "ws_bridge.h"
#include "ws_bridge_device.h"
#include "ws_bridge_entity_json.h"

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

namespace esphome {
namespace ws_bridge {

// Per-domain protocol logic, factored out of the platform classes so that the
// *source* of an entity's state and metadata is a parameter rather than
// baked-in `this`.
//
// Every helper takes the WsBridgeDevice that owns the protocol-facing identity
// (unique_id, device grouping, the hub pointer) plus the ESPHome entity the
// data actually comes from:
//
//   dev   the ws_bridge identity
//   src   where state and metadata are read from — the wrapped entity when
//         this is a wrapper, otherwise the ws_bridge entity itself
//   ovr   (declare only, may be null) the ws_bridge-side entity holding what
//         the YAML set explicitly; see add_common_entity_fields()
//
// For a plain `platform: ws_bridge` entity src and ovr are the same object and
// the fallback collapses to a no-op. For a wrapper they differ, and that is
// the point: device_class, unit_of_measurement, state_class, options and so on
// come from the wrapped entity without being re-declared in YAML, while
// anything written on the ws_bridge platform still wins.
//
// Sources are taken by non-const reference/pointer throughout: several ESPHome
// entity getters (get_state_class(), traits accessors, ...) are not
// const-qualified.
//
// Each domain provides up to four helpers:
//   ws_declare_*         send the ws_bridge/entity declaration
//   ws_push_state_*      send the current state, if the source has one
//   ws_subscribe_*       forward every future state change to the hub
//   ws_handle_command_*  apply an incoming HA command (writable domains only)

// HA-facing name for a wrapped entity. An explicit `name:` on the ws_bridge
// side always wins. Otherwise the source's own name is used — but only if it
// really has one: an ESPHome entity declared with just `id:` gets name=id and
// internal: true, and that id must not leak into HA as the entity name. The
// unique_id is the last resort.
inline std::string ws_ha_name(const EntityBase &src, const std::string &name_override, const std::string &fallback) {
  if (!name_override.empty())
    return name_override;
  if (src.has_own_name())
    return src.get_name().str();
  return fallback;
}

#ifdef USE_SENSOR
inline void ws_declare_sensor(WsBridgeDevice *dev, sensor::Sensor &src, sensor::Sensor *ovr,
                              const std::string &name, bool accuracy_overridden = false) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "sensor", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr, accuracy_overridden](JsonObject root) {
        add_common_entity_fields(root, src, ovr);

        StringRef uom = src.get_unit_of_measurement_ref();
        if (ovr != nullptr && !ovr->get_unit_of_measurement_ref().empty())
          uom = ovr->get_unit_of_measurement_ref();
        if (!uom.empty()) root["unit_of_measurement"] = uom.str();

        sensor::StateClass sc = src.get_state_class();
        if (ovr != nullptr && ovr->get_state_class() != sensor::STATE_CLASS_NONE) sc = ovr->get_state_class();
        if (sc != sensor::STATE_CLASS_NONE) root["state_class"] = LOG_STR_ARG(sensor::state_class_to_string(sc));

        // Sensor::get_accuracy_decimals() returns 0 both when never set and
        // when genuinely set to 0 — ESPHome tracks the real "was it
        // overridden" bit internally but doesn't expose it. Codegen sets
        // accuracy_overridden when YAML contained `accuracy_decimals:`
        // (including an explicit 0), so a wrapper can force fewer decimals
        // than a more precise source. Number min/max/step and select options
        // don't need this: their "unset" sentinels (NaN / empty list) are
        // not legitimate values, so presence in YAML is already visible.
        int8_t accuracy = src.get_accuracy_decimals();
        if (ovr != nullptr && accuracy_overridden) accuracy = ovr->get_accuracy_decimals();
        root["suggested_display_precision"] = accuracy;
      });
}

inline void ws_push_state_sensor(WsBridgeDevice *dev, sensor::Sensor &src) {
  if (src.has_state()) dev->get_ws_bridge_parent()->send_state_float(dev->get_ws_bridge_unique_id(), src.state);
}

inline void ws_subscribe_sensor(WsBridgeDevice *dev, sensor::Sensor *src) {
  src->add_on_state_callback(
      [dev](float state) { dev->get_ws_bridge_parent()->send_state_float(dev->get_ws_bridge_unique_id(), state); });
}
#endif  // USE_SENSOR

#ifdef USE_BINARY_SENSOR
inline void ws_declare_binary_sensor(WsBridgeDevice *dev, binary_sensor::BinarySensor &src,
                                     binary_sensor::BinarySensor *ovr, const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "binary_sensor", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) { add_common_entity_fields(root, src, ovr); });
}

inline void ws_push_state_binary_sensor(WsBridgeDevice *dev, binary_sensor::BinarySensor &src) {
  if (src.has_state()) dev->get_ws_bridge_parent()->send_state_bool(dev->get_ws_bridge_unique_id(), src.state);
}

inline void ws_subscribe_binary_sensor(WsBridgeDevice *dev, binary_sensor::BinarySensor *src) {
  src->add_on_state_callback(
      [dev](bool state) { dev->get_ws_bridge_parent()->send_state_bool(dev->get_ws_bridge_unique_id(), state); });
}
#endif  // USE_BINARY_SENSOR

#ifdef USE_TEXT_SENSOR
inline void ws_declare_text_sensor(WsBridgeDevice *dev, text_sensor::TextSensor &src, text_sensor::TextSensor *ovr,
                                   const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "text_sensor", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) { add_common_entity_fields(root, src, ovr); });
}

inline void ws_push_state_text_sensor(WsBridgeDevice *dev, text_sensor::TextSensor &src) {
  if (src.has_state()) dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), src.state);
}

inline void ws_subscribe_text_sensor(WsBridgeDevice *dev, text_sensor::TextSensor *src) {
  src->add_on_state_callback([dev](const std::string &state) {
    dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), state);
  });
}
#endif  // USE_TEXT_SENSOR

#ifdef USE_SWITCH
inline void ws_declare_switch(WsBridgeDevice *dev, switch_::Switch &src, switch_::Switch *ovr,
                              const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "switch", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) { add_common_entity_fields(root, src, ovr); });
}

inline void ws_push_state_switch(WsBridgeDevice *dev, switch_::Switch &src) {
  if (src.has_state()) dev->get_ws_bridge_parent()->send_state_bool(dev->get_ws_bridge_unique_id(), src.state);
}

inline void ws_subscribe_switch(WsBridgeDevice *dev, switch_::Switch *src) {
  src->add_on_state_callback(
      [dev](bool state) { dev->get_ws_bridge_parent()->send_state_bool(dev->get_ws_bridge_unique_id(), state); });
}

inline void ws_handle_command_switch(switch_::Switch *target, const WsCommand &command) {
  if (command.action == "turn_on") {
    target->turn_on();
  } else if (command.action == "turn_off") {
    target->turn_off();
  }
}
#endif  // USE_SWITCH

#ifdef USE_NUMBER
inline void ws_declare_number(WsBridgeDevice *dev, number::Number &src, number::Number *ovr,
                              const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "number", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) {
        add_common_entity_fields(root, src, ovr);
        // Each of min/max/step falls back to src independently, so a wrapper
        // can override the range without also having to restate the step.
        // NumberTraits default-initializes all three to NAN and codegen leaves
        // them NaN when the YAML key was absent, so NaN means "inherit".
        float min_value = src.traits.get_min_value();
        float max_value = src.traits.get_max_value();
        float step = src.traits.get_step();
        if (ovr != nullptr) {
          if (!std::isnan(ovr->traits.get_min_value())) min_value = ovr->traits.get_min_value();
          if (!std::isnan(ovr->traits.get_max_value())) max_value = ovr->traits.get_max_value();
          if (!std::isnan(ovr->traits.get_step())) step = ovr->traits.get_step();
        }
        root["min"] = min_value;
        root["max"] = max_value;
        root["step"] = step;
      });
}

inline void ws_push_state_number(WsBridgeDevice *dev, number::Number &src) {
  if (src.has_state()) dev->get_ws_bridge_parent()->send_state_float(dev->get_ws_bridge_unique_id(), src.state);
}

inline void ws_subscribe_number(WsBridgeDevice *dev, number::Number *src) {
  src->add_on_state_callback(
      [dev](float state) { dev->get_ws_bridge_parent()->send_state_float(dev->get_ws_bridge_unique_id(), state); });
}

inline void ws_handle_command_number(number::Number *target, const WsCommand &command) {
  if (command.action == "set_value" && command.has_value) {
    target->make_call().set_value(command.value_float).perform();
  }
}
#endif  // USE_NUMBER

#ifdef USE_SELECT
inline void ws_declare_select(WsBridgeDevice *dev, select::Select &src, select::Select *ovr,
                              const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "select", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) {
        add_common_entity_fields(root, src, ovr);
        // An empty options list is SelectTraits' own default (see
        // FixedVector's default constructor) and select_schema() requires
        // `options:` whenever select_id: isn't set, so "ovr has options" is
        // an unambiguous "was this explicitly given" signal — no separate
        // flag needed, unlike accuracy_decimals.
        auto &traits = (ovr != nullptr && !ovr->traits.get_options().empty()) ? ovr->traits : src.traits;
        JsonArray options = root["options"].to<JsonArray>();
        for (const char *option : traits.get_options()) options.add(option);
      });
}

inline void ws_push_state_select(WsBridgeDevice *dev, select::Select &src) {
  if (src.has_state())
    dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), src.current_option().str());
}

inline void ws_subscribe_select(WsBridgeDevice *dev, select::Select *src) {
  src->add_on_state_callback([dev, src](size_t index) {
    dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), src->option_at(index));
  });
}

inline void ws_handle_command_select(select::Select *target, const WsCommand &command) {
  if (command.action == "select_option" && command.has_value) {
    target->make_call().set_option(command.value_string).perform();
  }
}
#endif  // USE_SELECT

#ifdef USE_BUTTON
inline void ws_declare_button(WsBridgeDevice *dev, button::Button &src, button::Button *ovr,
                              const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "button", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) { add_common_entity_fields(root, src, ovr); });
}

inline void ws_handle_command_button(button::Button *target, const WsCommand &command) {
  if (command.action == "press") target->press();
}
#endif  // USE_BUTTON

#ifdef USE_UPDATE
inline void ws_fill_state_update(update::UpdateEntity &src, JsonObject value) {
  const auto &info = src.update_info;
  if (!info.current_version.empty())
    value["installed_version"] = info.current_version;
  if (!info.latest_version.empty())
    value["latest_version"] = info.latest_version;
  value["in_progress"] = src.state == update::UPDATE_STATE_INSTALLING;
  if (info.has_progress)
    value["progress"] = static_cast<int>(info.progress);
  if (!info.title.empty())
    value["title"] = info.title;
  if (!info.summary.empty())
    value["summary"] = info.summary;
  if (!info.release_url.empty())
    value["release_url"] = info.release_url;
}

inline void ws_send_state_update(WsBridgeDevice *dev, update::UpdateEntity &src) {
  dev->get_ws_bridge_parent()->send_state_object(dev->get_ws_bridge_unique_id(),
                                                 [&src](JsonObject value) { ws_fill_state_update(src, value); });
}

// No `ovr`: WsBridgeUpdate is a bare Component rather than an UpdateEntity, so
// it has no entity metadata of its own to override with — `name:` is its only
// presentation option and that is resolved by the caller via ws_ha_name().
inline void ws_declare_update(WsBridgeDevice *dev, update::UpdateEntity &src, const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(dev->get_ws_bridge_unique_id(), "update", name,
                                                   dev->get_ws_bridge_device_id(), dev->get_ws_bridge_device_name(),
                                                   [&src](JsonObject root) {
                                                     add_common_entity_fields(root, src, nullptr);
                                                     if (root["device_class"].isNull())
                                                       root["device_class"] = "firmware";
                                                   });
}

inline void ws_push_state_update(WsBridgeDevice *dev, update::UpdateEntity &src) {
  if (src.has_state()) ws_send_state_update(dev, src);
}

inline void ws_subscribe_update(WsBridgeDevice *dev, update::UpdateEntity *src) {
  src->add_on_state_callback([dev, src]() { ws_send_state_update(dev, *src); });
}

inline void ws_handle_command_update(update::UpdateEntity *target, const WsCommand &command) {
  if (command.action == "install") {
    target->perform();
  } else if (command.action == "check") {
    target->check();
  }
}
#endif  // USE_UPDATE

}  // namespace ws_bridge
}  // namespace esphome
