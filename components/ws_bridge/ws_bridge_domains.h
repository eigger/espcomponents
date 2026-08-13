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
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif
#ifdef USE_COVER
#include "esphome/components/cover/cover.h"
#endif
#ifdef USE_CLIMATE
#include "esphome/components/climate/climate.h"
#endif
#ifdef USE_FAN
#include "esphome/components/fan/fan.h"
#endif
#ifdef USE_TEXT
#include "esphome/components/text/text.h"
#endif
#ifdef USE_LOCK
#include "esphome/components/lock/lock.h"
#endif
#ifdef USE_VALVE
#include "esphome/components/valve/valve.h"
#endif
#ifdef USE_EVENT
#include "esphome/components/event/event.h"
#endif
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
  if (command.action == "set_value" && command.value_is_number) {
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
  if (command.action == "select_option" && command.value_is_string) {
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

#ifdef USE_COVER
inline void ws_fill_state_cover(cover::Cover &src, JsonObject value) {
  switch (src.current_operation) {
    case cover::COVER_OPERATION_OPENING:
      value["state"] = "opening";
      break;
    case cover::COVER_OPERATION_CLOSING:
      value["state"] = "closing";
      break;
    case cover::COVER_OPERATION_IDLE:
    default:
      // ESPHome treats only position==0 as closed (cover.cpp); anything else is open.
      value["state"] = src.is_fully_closed() ? "closed" : "open";
      break;
  }
  value["position"] = static_cast<int>(roundf(src.position * 100.0f));
  auto traits = src.get_traits();
  if (traits.get_supports_tilt())
    value["tilt_position"] = static_cast<int>(roundf(src.tilt * 100.0f));
}

inline void ws_send_state_cover(WsBridgeDevice *dev, cover::Cover &src) {
  dev->get_ws_bridge_parent()->send_state_object(dev->get_ws_bridge_unique_id(),
                                                 [&src](JsonObject value) { ws_fill_state_cover(src, value); });
}

inline void ws_declare_cover(WsBridgeDevice *dev, cover::Cover &src, cover::Cover *ovr, const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "cover", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) {
        add_common_entity_fields(root, src, ovr);
        auto traits = src.get_traits();
        JsonArray features = root["features"].to<JsonArray>();
        features.add("open");
        features.add("close");
        if (traits.get_supports_stop())
          features.add("stop");
        if (traits.get_supports_position())
          features.add("set_position");
        if (traits.get_supports_tilt()) {
          features.add("open_tilt");
          features.add("close_tilt");
          features.add("set_tilt_position");
          if (traits.get_supports_stop())
            features.add("stop_tilt");
        }
      });
}

inline void ws_push_state_cover(WsBridgeDevice *dev, cover::Cover &src) { ws_send_state_cover(dev, src); }

inline void ws_subscribe_cover(WsBridgeDevice *dev, cover::Cover *src) {
  src->add_on_state_callback([dev, src]() { ws_send_state_cover(dev, *src); });
}

inline void ws_handle_command_cover(cover::Cover *target, const WsCommand &command) {
  if (command.action == "open_cover") {
    target->make_call().set_command_open().perform();
  } else if (command.action == "close_cover") {
    target->make_call().set_command_close().perform();
  } else if (command.action == "stop_cover" || command.action == "stop_cover_tilt") {
    target->make_call().set_command_stop().perform();
  } else if (command.action == "set_cover_position") {
    float position;
    if (command.param_float("position", position))
      target->make_call().set_position(position / 100.0f).perform();
  } else if (command.action == "open_cover_tilt") {
    target->make_call().set_tilt(cover::COVER_OPEN).perform();
  } else if (command.action == "close_cover_tilt") {
    target->make_call().set_tilt(cover::COVER_CLOSED).perform();
  } else if (command.action == "set_cover_tilt_position") {
    float tilt;
    if (command.param_float("tilt_position", tilt))
      target->make_call().set_tilt(tilt / 100.0f).perform();
  }
}
#endif  // USE_COVER

#ifdef USE_FAN
inline int ws_fan_percentage_(fan::Fan &src) {
  auto traits = src.get_traits();
  int count = traits.supported_speed_count();
  if (!src.state || !traits.supports_speed() || count <= 0)
    return 0;
  return static_cast<int>(roundf(100.0f * src.speed / count));
}

inline void ws_fill_state_fan(fan::Fan &src, JsonObject value) {
  value["state"] = src.state ? "on" : "off";
  auto traits = src.get_traits();
  if (traits.supports_speed())
    value["percentage"] = ws_fan_percentage_(src);
  if (traits.supports_oscillation())
    value["oscillating"] = src.oscillating;
  if (traits.supports_direction())
    value["direction"] = src.direction == fan::FanDirection::REVERSE ? "reverse" : "forward";
  if (traits.supports_preset_modes() && src.has_preset_mode())
    value["preset_mode"] = src.get_preset_mode();
}

inline void ws_send_state_fan(WsBridgeDevice *dev, fan::Fan &src) {
  dev->get_ws_bridge_parent()->send_state_object(dev->get_ws_bridge_unique_id(),
                                                 [&src](JsonObject value) { ws_fill_state_fan(src, value); });
}

inline void ws_declare_fan(WsBridgeDevice *dev, fan::Fan &src, fan::Fan *ovr, const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "fan", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) {
        add_common_entity_fields(root, src, ovr);
        auto traits = src.get_traits();
        JsonArray features = root["features"].to<JsonArray>();
        features.add("turn_on");
        features.add("turn_off");
        if (traits.supports_speed()) {
          features.add("set_speed");
          root["speed_count"] = traits.supported_speed_count();
        }
        if (traits.supports_oscillation())
          features.add("oscillate");
        if (traits.supports_direction())
          features.add("direction");
        if (traits.supports_preset_modes()) {
          features.add("preset_mode");
          JsonArray presets = root["preset_modes"].to<JsonArray>();
          for (const auto &mode : traits.supported_preset_modes())
            presets.add(mode);
        }
      });
}

inline void ws_push_state_fan(WsBridgeDevice *dev, fan::Fan &src) { ws_send_state_fan(dev, src); }

inline void ws_subscribe_fan(WsBridgeDevice *dev, fan::Fan *src) {
  src->add_on_state_callback([dev, src]() { ws_send_state_fan(dev, *src); });
}

inline void ws_fan_apply_percentage_(fan::Fan *target, float percentage) {
  auto traits = target->get_traits();
  int count = traits.supported_speed_count();
  if (percentage <= 0.0f || count <= 0) {
    target->turn_off().perform();
    return;
  }
  int speed = static_cast<int>(roundf(percentage / 100.0f * count));
  if (speed < 1)
    speed = 1;
  if (speed > count)
    speed = count;
  target->make_call().set_state(true).set_speed(speed).perform();
}

inline void ws_handle_command_fan(fan::Fan *target, const WsCommand &command) {
  if (command.action == "turn_on") {
    float percentage;
    std::string preset;
    auto call = target->make_call().set_state(true);
    if (command.param_float("percentage", percentage)) {
      auto traits = target->get_traits();
      int count = traits.supported_speed_count();
      if (count > 0) {
        int speed = static_cast<int>(roundf(percentage / 100.0f * count));
        if (speed < 1)
          speed = 1;
        if (speed > count)
          speed = count;
        call.set_speed(speed);
      }
    }
    if (command.param_string("preset_mode", preset))
      call.set_preset_mode(preset);
    call.perform();
  } else if (command.action == "turn_off") {
    target->turn_off().perform();
  } else if (command.action == "set_percentage") {
    float percentage;
    if (command.param_float("percentage", percentage))
      ws_fan_apply_percentage_(target, percentage);
  } else if (command.action == "set_preset_mode") {
    std::string preset;
    if (command.param_string("preset_mode", preset))
      target->make_call().set_preset_mode(preset).perform();
  } else if (command.action == "oscillate") {
    bool oscillating;
    if (command.param_bool("oscillating", oscillating))
      target->make_call().set_oscillating(oscillating).perform();
  } else if (command.action == "set_direction") {
    std::string direction;
    if (command.param_string("direction", direction)) {
      auto dir = direction == "reverse" ? fan::FanDirection::REVERSE : fan::FanDirection::FORWARD;
      target->make_call().set_direction(dir).perform();
    }
  }
}
#endif  // USE_FAN

#ifdef USE_LIGHT
inline const char *ws_light_color_mode_to_ha_(light::ColorMode mode) {
  using light::ColorMode;
  switch (mode) {
    case ColorMode::ON_OFF:
      return "onoff";
    case ColorMode::BRIGHTNESS:
      return "brightness";
    case ColorMode::WHITE:
      return "white";
    case ColorMode::COLOR_TEMPERATURE:
    case ColorMode::COLD_WARM_WHITE:
      return "color_temp";
    case ColorMode::RGB:
      return "rgb";
    case ColorMode::RGB_WHITE:
      return "rgbw";
    case ColorMode::RGB_COLOR_TEMPERATURE:
    case ColorMode::RGB_COLD_WARM_WHITE:
      return "rgbww";
    default:
      return nullptr;
  }
}

inline void ws_fill_state_light(light::LightState &src, JsonObject value) {
  const auto &v = src.remote_values;
  bool on = v.is_on();
  value["state"] = on ? "on" : "off";
  if (!on)
    return;

  auto traits = src.get_traits();
  if (traits.supports_color_capability(light::ColorCapability::BRIGHTNESS))
    value["brightness"] = light::to_uint8_scale(v.get_brightness());

  if (const char *mode = ws_light_color_mode_to_ha_(v.get_color_mode()))
    value["color_mode"] = mode;

  // ESPHome stores RGB normalized with color_brightness separate — HA wants
  // scaled 0–255 channels (see homeassistant/components/esphome/light.py).
  const float color_bri = v.get_color_brightness();

  using light::ColorMode;
  switch (v.get_color_mode()) {
    case ColorMode::COLOR_TEMPERATURE:
    case ColorMode::RGB_COLOR_TEMPERATURE: {
      float mireds = v.get_color_temperature();
      if (mireds > 0.0f)
        value["color_temp_kelvin"] = static_cast<int>(roundf(1000000.0f / mireds));
      if (v.get_color_mode() == ColorMode::RGB_COLOR_TEMPERATURE) {
        JsonArray rgb = value["rgb_color"].to<JsonArray>();
        rgb.add(light::to_uint8_scale(v.get_red() * color_bri));
        rgb.add(light::to_uint8_scale(v.get_green() * color_bri));
        rgb.add(light::to_uint8_scale(v.get_blue() * color_bri));
      }
      break;
    }
    case ColorMode::COLD_WARM_WHITE:
    case ColorMode::RGB_COLD_WARM_WHITE: {
      float min_m = traits.get_min_mireds();
      float max_m = traits.get_max_mireds();
      float cold = v.get_cold_white();
      float warm = v.get_warm_white();
      float sum = cold + warm;
      if (sum > 0.0f && max_m > min_m) {
        float mireds = min_m + (1.0f - cold / sum) * (max_m - min_m);
        if (mireds > 0.0f)
          value["color_temp_kelvin"] = static_cast<int>(roundf(1000000.0f / mireds));
      }
      if (v.get_color_mode() == ColorMode::RGB_COLD_WARM_WHITE) {
        JsonArray rgbww = value["rgbww_color"].to<JsonArray>();
        rgbww.add(light::to_uint8_scale(v.get_red() * color_bri));
        rgbww.add(light::to_uint8_scale(v.get_green() * color_bri));
        rgbww.add(light::to_uint8_scale(v.get_blue() * color_bri));
        rgbww.add(light::to_uint8_scale(v.get_cold_white()));
        rgbww.add(light::to_uint8_scale(v.get_warm_white()));
      }
      break;
    }
    case ColorMode::RGB: {
      JsonArray rgb = value["rgb_color"].to<JsonArray>();
      rgb.add(light::to_uint8_scale(v.get_red() * color_bri));
      rgb.add(light::to_uint8_scale(v.get_green() * color_bri));
      rgb.add(light::to_uint8_scale(v.get_blue() * color_bri));
      break;
    }
    case ColorMode::RGB_WHITE: {
      JsonArray rgbw = value["rgbw_color"].to<JsonArray>();
      rgbw.add(light::to_uint8_scale(v.get_red() * color_bri));
      rgbw.add(light::to_uint8_scale(v.get_green() * color_bri));
      rgbw.add(light::to_uint8_scale(v.get_blue() * color_bri));
      rgbw.add(light::to_uint8_scale(v.get_white()));
      break;
    }
    case ColorMode::WHITE:
      value["brightness"] = light::to_uint8_scale(v.get_white() * v.get_brightness());
      break;
    default:
      break;
  }

  StringRef effect = src.get_effect_name();
  if (!effect.empty() && effect != "None")
    value["effect"] = effect;  // ArduinoJson has convertToJson(StringRef)
}

inline void ws_send_state_light(WsBridgeDevice *dev, light::LightState &src) {
  dev->get_ws_bridge_parent()->send_state_object(dev->get_ws_bridge_unique_id(),
                                                 [&src](JsonObject value) { ws_fill_state_light(src, value); });
}

inline void ws_declare_light(WsBridgeDevice *dev, light::LightState &src, const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "light", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src](JsonObject root) {
        add_common_entity_fields(root, src, nullptr);
        auto traits = src.get_traits();
        JsonArray modes = root["supported_color_modes"].to<JsonArray>();
        bool has_colorful = false;
        for (auto mode : traits.get_supported_color_modes()) {
          if (mode == light::ColorMode::ON_OFF || mode == light::ColorMode::BRIGHTNESS)
            continue;
          if (const char *ha = ws_light_color_mode_to_ha_(mode)) {
            modes.add(ha);
            has_colorful = true;
          }
        }
        if (!has_colorful) {
          if (traits.supports_color_mode(light::ColorMode::BRIGHTNESS))
            modes.add("brightness");
          else
            modes.add("onoff");
        }

        float min_m = traits.get_min_mireds();
        float max_m = traits.get_max_mireds();
        if (min_m > 0.0f && max_m > 0.0f) {
          // HA wants kelvin; ESPHome traits store mireds (min mireds = max kelvin).
          root["max_color_temp_kelvin"] = static_cast<int>(roundf(1000000.0f / min_m));
          root["min_color_temp_kelvin"] = static_cast<int>(roundf(1000000.0f / max_m));
        }

        if (src.supports_effects()) {
          JsonArray effects = root["effect_list"].to<JsonArray>();
          for (auto *effect : src.get_effects())
            effects.add(effect->get_name());
        }

        JsonArray features = root["features"].to<JsonArray>();
        if (traits.supports_color_capability(light::ColorCapability::BRIGHTNESS)) {
          features.add("transition");
          features.add("flash");
        }
        if (src.supports_effects())
          features.add("effect");
        if (features.size() == 0)
          root.remove("features");
      });
}

inline void ws_push_state_light(WsBridgeDevice *dev, light::LightState &src) { ws_send_state_light(dev, src); }

// LightState no longer accepts std::function callbacks — platforms that want
// state pushes must implement LightRemoteValuesListener and register `this`.
inline void ws_subscribe_light(light::LightState *src, light::LightRemoteValuesListener *listener) {
  src->add_remote_values_listener(listener);
}

inline void ws_apply_rgb_(light::LightCall &call, float r, float g, float b) {
  // HA sends absolute 0–1 RGB; ESPHome normalizes channels and keeps the max
  // in color_brightness (otherwise validate_()/normalize_color() discards it).
  float color_bri = fmaxf(r, fmaxf(g, b));
  if (color_bri > 0.0f) {
    call.set_rgb(r / color_bri, g / color_bri, b / color_bri);
    call.set_color_brightness(color_bri);
  } else {
    call.set_rgb(0.0f, 0.0f, 0.0f);
    call.set_color_brightness(0.0f);
  }
}

inline void ws_hs_to_rgb_(float h, float s_pct, float &r, float &g, float &b) {
  float s = s_pct / 100.0f;
  float c = s;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m = 1.0f - c;
  float rp = 0, gp = 0, bp = 0;
  if (h < 60.0f) {
    rp = c;
    gp = x;
  } else if (h < 120.0f) {
    rp = x;
    gp = c;
  } else if (h < 180.0f) {
    gp = c;
    bp = x;
  } else if (h < 240.0f) {
    gp = x;
    bp = c;
  } else if (h < 300.0f) {
    rp = x;
    bp = c;
  } else {
    rp = c;
    bp = x;
  }
  r = rp + m;
  g = gp + m;
  b = bp + m;
}

inline void ws_handle_command_light(light::LightState *target, const WsCommand &command) {
  if (command.action == "turn_off") {
    auto call = target->turn_off();
    float transition;
    if (command.param_float("transition", transition))
      call.set_transition_length(static_cast<uint32_t>(transition * 1000.0f));
    call.perform();
    return;
  }
  if (command.action != "turn_on")
    return;

  auto call = target->turn_on();
  float brightness;
  if (command.param_float("brightness", brightness))
    call.set_brightness(brightness / 255.0f);

  float kelvin;
  if (command.param_float("color_temp_kelvin", kelvin) && kelvin > 0.0f)
    call.set_color_temperature(1000000.0f / kelvin);

  std::vector<float> hs;
  if (command.param_array("hs_color", hs, 2)) {
    float r, g, b;
    ws_hs_to_rgb_(hs[0], hs[1], r, g, b);
    ws_apply_rgb_(call, r, g, b);
  }

  std::vector<float> rgb;
  if (command.param_array("rgb_color", rgb, 3))
    ws_apply_rgb_(call, rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f);

  std::vector<float> rgbw;
  if (command.param_array("rgbw_color", rgbw, 4)) {
    ws_apply_rgb_(call, rgbw[0] / 255.0f, rgbw[1] / 255.0f, rgbw[2] / 255.0f);
    call.set_white(rgbw[3] / 255.0f);
  }

  std::vector<float> rgbww;
  if (command.param_array("rgbww_color", rgbww, 5)) {
    ws_apply_rgb_(call, rgbww[0] / 255.0f, rgbww[1] / 255.0f, rgbww[2] / 255.0f);
    call.set_cold_white(rgbww[3] / 255.0f);
    call.set_warm_white(rgbww[4] / 255.0f);
  }

  float white;
  if (command.param_float("white", white))
    call.set_white(white / 255.0f);

  std::string effect;
  if (command.param_string("effect", effect))
    call.set_effect(effect);

  float transition;
  if (command.param_float("transition", transition))
    call.set_transition_length(static_cast<uint32_t>(transition * 1000.0f));

  std::string flash;
  if (command.param_string("flash", flash)) {
    uint32_t ms = flash == "long" ? 5000 : 1000;
    call.set_flash_length(ms);
  }

  call.perform();
}
#endif  // USE_LIGHT

#ifdef USE_TEXT
inline void ws_declare_text(WsBridgeDevice *dev, text::Text &src, text::Text *ovr, const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "text", name, dev->get_ws_bridge_device_id(), dev->get_ws_bridge_device_name(),
      [&src, ovr](JsonObject root) {
        add_common_entity_fields(root, src, ovr);
        // min/max are *string lengths* here, not a value range like number's.
        // Read from src only (like cover/fan/light traits, unlike number's
        // per-field fallback): TextTraits has no "unset" sentinel — ESPHome's
        // register_text() always writes both, defaulting to 0/255 — so a
        // wrapper could not tell an inherited default from an explicit one.
        // Wrapping therefore reports the wrapped text's length limits, pattern
        // and mode as-is.
        root["min"] = src.traits.get_min_length();
        root["max"] = src.traits.get_max_length();
        StringRef pattern = src.traits.get_pattern_ref();
        if (!pattern.empty())
          root["pattern"] = pattern;  // ArduinoJson has convertToJson(StringRef)
        root["mode"] = src.traits.get_mode() == text::TEXT_MODE_PASSWORD ? "password" : "text";
      });
}

inline void ws_push_state_text(WsBridgeDevice *dev, text::Text &src) {
  if (src.has_state())
    dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), src.state);
}

inline void ws_subscribe_text(WsBridgeDevice *dev, text::Text *src) {
  src->add_on_state_callback([dev](const std::string &state) {
    dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), state);
  });
}

inline void ws_handle_command_text(text::Text *target, const WsCommand &command) {
  if (command.action == "set_value" && command.value_is_string) {
    target->make_call().set_value(command.value_string).perform();
  }
}
#endif  // USE_TEXT

#ifdef USE_LOCK
// Null for LOCK_STATE_NONE — a lock that has never published. HA has no
// equivalent "no state yet", so those are simply not sent.
inline const char *ws_lock_state_to_ha_(lock::LockState state) {
  switch (state) {
    case lock::LOCK_STATE_LOCKED:
      return "locked";
    case lock::LOCK_STATE_UNLOCKED:
      return "unlocked";
    case lock::LOCK_STATE_JAMMED:
      return "jammed";
    case lock::LOCK_STATE_LOCKING:
      return "locking";
    case lock::LOCK_STATE_UNLOCKING:
      return "unlocking";
    case lock::LOCK_STATE_OPENING:
      return "opening";
    case lock::LOCK_STATE_OPEN:
      return "open";
    default:
      return nullptr;
  }
}

// `code_format` is a ws_bridge-only YAML option rather than something read off
// the source: ESPHome's lock domain has LockTraits::requires_code but no way to
// carry the code itself, so the regex HA validates against can only come from
// here. Empty means "no code prompt in HA".
inline void ws_declare_lock(WsBridgeDevice *dev, lock::Lock &src, lock::Lock *ovr, const std::string &name,
                            const std::string &code_format) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "lock", name, dev->get_ws_bridge_device_id(), dev->get_ws_bridge_device_name(),
      [&src, ovr, &code_format](JsonObject root) {
        add_common_entity_fields(root, src, ovr);
        // lock/unlock are always available, so "open" (unlatch) is the only
        // feature flag the protocol defines for this domain.
        if (src.traits.get_supports_open()) {
          JsonArray features = root["features"].to<JsonArray>();
          features.add("open");
        }
        if (!code_format.empty())
          root["code_format"] = code_format;
      });
}

inline void ws_push_state_lock(WsBridgeDevice *dev, lock::Lock &src) {
  if (const char *state = ws_lock_state_to_ha_(src.state))
    dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), state);
}

inline void ws_subscribe_lock(WsBridgeDevice *dev, lock::Lock *src) {
  src->add_on_state_callback([dev](lock::LockState state) {
    if (const char *ha = ws_lock_state_to_ha_(state))
      dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), ha);
  });
}

// HA may send params.code alongside lock/unlock/open when code_format is
// declared. It is deliberately ignored: there is nothing in ESPHome's lock API
// to hand it to. Treat code_format as a UI-side confirmation prompt only, never
// as device-enforced authentication.
inline void ws_handle_command_lock(lock::Lock *target, const WsCommand &command) {
  if (command.action == "lock") {
    target->lock();
  } else if (command.action == "unlock") {
    target->unlock();
  } else if (command.action == "open") {
    target->open();
  }
}
#endif  // USE_LOCK

#ifdef USE_VALVE
inline void ws_fill_state_valve(valve::Valve &src, JsonObject value) {
  switch (src.current_operation) {
    case valve::VALVE_OPERATION_OPENING:
      value["state"] = "opening";
      break;
    case valve::VALVE_OPERATION_CLOSING:
      value["state"] = "closing";
      break;
    case valve::VALVE_OPERATION_IDLE:
    default:
      // Same rule as cover: only position==0 counts as closed (valve.cpp).
      value["state"] = src.is_fully_closed() ? "closed" : "open";
      break;
  }
  // A valve that declared reports_position: false must not report a position
  // at all — the protocol keeps the two modes separate.
  if (src.get_traits().get_supports_position())
    value["position"] = static_cast<int>(roundf(src.position * 100.0f));
}

inline void ws_send_state_valve(WsBridgeDevice *dev, valve::Valve &src) {
  dev->get_ws_bridge_parent()->send_state_object(dev->get_ws_bridge_unique_id(),
                                                 [&src](JsonObject value) { ws_fill_state_valve(src, value); });
}

inline void ws_declare_valve(WsBridgeDevice *dev, valve::Valve &src, valve::Valve *ovr, const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "valve", name, dev->get_ws_bridge_device_id(), dev->get_ws_bridge_device_name(),
      [&src, ovr](JsonObject root) {
        add_common_entity_fields(root, src, ovr);
        auto traits = src.get_traits();
        root["reports_position"] = traits.get_supports_position();
        JsonArray features = root["features"].to<JsonArray>();
        features.add("open");
        features.add("close");
        if (traits.get_supports_stop())
          features.add("stop");
        if (traits.get_supports_position())
          features.add("set_position");
      });
}

inline void ws_push_state_valve(WsBridgeDevice *dev, valve::Valve &src) { ws_send_state_valve(dev, src); }

inline void ws_subscribe_valve(WsBridgeDevice *dev, valve::Valve *src) {
  src->add_on_state_callback([dev, src]() { ws_send_state_valve(dev, *src); });
}

inline void ws_handle_command_valve(valve::Valve *target, const WsCommand &command) {
  if (command.action == "open_valve") {
    target->make_call().set_command_open().perform();
  } else if (command.action == "close_valve") {
    target->make_call().set_command_close().perform();
  } else if (command.action == "stop_valve") {
    target->make_call().set_command_stop().perform();
  } else if (command.action == "set_valve_position") {
    float position;
    if (command.param_float("position", position))
      target->make_call().set_position(position / 100.0f).perform();
  }
}
#endif  // USE_VALVE

#ifdef USE_EVENT
inline void ws_declare_event(WsBridgeDevice *dev, event::Event &src, event::Event *ovr, const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "event", name, dev->get_ws_bridge_device_id(), dev->get_ws_bridge_device_name(),
      [&src, ovr](JsonObject root) {
        add_common_entity_fields(root, src, ovr);
        // Read from src, never from ovr: the state pushes come from src's
        // trigger(), and HA drops any event_type that wasn't declared. A
        // wrapper's own list could disagree with the source's and silently
        // swallow events.
        JsonArray types = root["event_types"].to<JsonArray>();
        for (const char *type : src.get_event_types())
          types.add(type);
      });
}

// No ws_push_state_event(): events are fire-and-forget and HA restores no last
// state for them, so re-pushing the last event_type on every (re)declare would
// replay a doorbell press that already happened. There is no
// ws_handle_command_event() either — the domain is read-only.
inline void ws_subscribe_event(WsBridgeDevice *dev, event::Event *src) {
  src->add_on_event_callback([dev](StringRef event_type) {
    dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), event_type.str());
  });
}
#endif  // USE_EVENT

#if defined(USE_DATETIME_DATE) || defined(USE_DATETIME_TIME) || defined(USE_DATETIME_DATETIME)
// date/time/datetime share everything but the platform string and the state
// format — none of the three has any declare-only field of its own.
inline void ws_declare_datetime_(WsBridgeDevice *dev, datetime::DateTimeBase &src, datetime::DateTimeBase *ovr,
                                 const std::string &name, const char *platform) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), platform, name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) { add_common_entity_fields(root, src, ovr); });
}

// Formats into a stack buffer (24 bytes covers the longest of these, the 19
// character datetime). Each caller must pass a format naming only the fields
// its entity actually owns: DateEntity::state_as_esptime() leaves
// hour/minute/second untouched and TimeEntity leaves the date fields untouched.
inline void ws_send_state_datetime_(WsBridgeDevice *dev, datetime::DateTimeBase &src, const char *format) {
  ESPTime time = src.state_as_esptime();
  char buf[24];
  if (time.strftime(buf, sizeof(buf), format) == 0)
    return;
  dev->get_ws_bridge_parent()->send_state_string(dev->get_ws_bridge_unique_id(), buf);
}

// HA sends full ISO 8601 ("2026-08-13T21:30:00+09:00"); ESPTime::strptime()
// accepts only "YYYY-MM-DD[ HH:MM[:SS]]" and "HH:MM[:SS]" — no 'T' separator,
// no fractional seconds, no timezone. We keep the wall-clock digits and drop
// any offset/Z — the round trip assumes HA sends **local** wall time (see
// hass-ws-bridge datetime.async_set_value), matching the tz-naive strings we
// publish back (HA re-attaches its local zone on receive).
inline std::string ws_iso_to_esptime_string_(const std::string &iso) {
  std::string out = iso;
  size_t separator = out.find('T');
  if (separator != std::string::npos)
    out[separator] = ' ';

  // Everything past the seconds field has to go. Scanning from the first ':'
  // keeps the date's own '-' separators from being read as a negative UTC
  // offset; a date-only value has no such suffix to strip in the first place.
  size_t time_start = out.find(':');
  if (time_start != std::string::npos) {
    size_t cut = out.find_first_of(".Z+-", time_start);
    if (cut != std::string::npos)
      out.erase(cut);
  }
  while (!out.empty() && out.back() == ' ')
    out.pop_back();
  return out;
}
#endif  // any USE_DATETIME_*

#ifdef USE_DATETIME_DATE
inline void ws_declare_date(WsBridgeDevice *dev, datetime::DateEntity &src, datetime::DateEntity *ovr,
                            const std::string &name) {
  ws_declare_datetime_(dev, src, ovr, name, "date");
}

inline void ws_push_state_date(WsBridgeDevice *dev, datetime::DateEntity &src) {
  if (src.has_state())
    ws_send_state_datetime_(dev, src, "%Y-%m-%d");
}

inline void ws_subscribe_date(WsBridgeDevice *dev, datetime::DateEntity *src) {
  src->add_on_state_callback([dev, src]() { ws_push_state_date(dev, *src); });
}

inline void ws_handle_command_date(datetime::DateEntity *target, const WsCommand &command) {
  if (command.action == "set_value" && command.value_is_string) {
    target->make_call().set_date(ws_iso_to_esptime_string_(command.value_string)).perform();
  }
}
#endif  // USE_DATETIME_DATE

#ifdef USE_DATETIME_TIME
inline void ws_declare_time(WsBridgeDevice *dev, datetime::TimeEntity &src, datetime::TimeEntity *ovr,
                            const std::string &name) {
  ws_declare_datetime_(dev, src, ovr, name, "time");
}

inline void ws_push_state_time(WsBridgeDevice *dev, datetime::TimeEntity &src) {
  if (src.has_state())
    ws_send_state_datetime_(dev, src, "%H:%M:%S");
}

inline void ws_subscribe_time(WsBridgeDevice *dev, datetime::TimeEntity *src) {
  src->add_on_state_callback([dev, src]() { ws_push_state_time(dev, *src); });
}

inline void ws_handle_command_time(datetime::TimeEntity *target, const WsCommand &command) {
  if (command.action == "set_value" && command.value_is_string) {
    target->make_call().set_time(ws_iso_to_esptime_string_(command.value_string)).perform();
  }
}
#endif  // USE_DATETIME_TIME

#ifdef USE_DATETIME_DATETIME
inline void ws_declare_datetime(WsBridgeDevice *dev, datetime::DateTimeEntity &src, datetime::DateTimeEntity *ovr,
                                const std::string &name) {
  ws_declare_datetime_(dev, src, ovr, name, "datetime");
}

inline void ws_push_state_datetime(WsBridgeDevice *dev, datetime::DateTimeEntity &src) {
  if (src.has_state())
    ws_send_state_datetime_(dev, src, "%Y-%m-%dT%H:%M:%S");
}

inline void ws_subscribe_datetime(WsBridgeDevice *dev, datetime::DateTimeEntity *src) {
  src->add_on_state_callback([dev, src]() { ws_push_state_datetime(dev, *src); });
}

inline void ws_handle_command_datetime(datetime::DateTimeEntity *target, const WsCommand &command) {
  if (command.action == "set_value" && command.value_is_string) {
    target->make_call().set_datetime(ws_iso_to_esptime_string_(command.value_string)).perform();
  }
}
#endif  // USE_DATETIME_DATETIME

#ifdef USE_CLIMATE
inline const char *ws_ha_climate_mode_(climate::ClimateMode mode) {
  switch (mode) {
    case climate::CLIMATE_MODE_OFF:
      return "off";
    case climate::CLIMATE_MODE_HEAT_COOL:
      return "heat_cool";
    case climate::CLIMATE_MODE_COOL:
      return "cool";
    case climate::CLIMATE_MODE_HEAT:
      return "heat";
    case climate::CLIMATE_MODE_FAN_ONLY:
      return "fan_only";
    case climate::CLIMATE_MODE_DRY:
      return "dry";
    case climate::CLIMATE_MODE_AUTO:
      return "auto";
    default:
      return nullptr;
  }
}

inline const char *ws_ha_climate_action_(climate::ClimateAction action) {
  switch (action) {
    case climate::CLIMATE_ACTION_OFF:
      return "off";
    case climate::CLIMATE_ACTION_COOLING:
      return "cooling";
    case climate::CLIMATE_ACTION_HEATING:
      return "heating";
    case climate::CLIMATE_ACTION_IDLE:
      return "idle";
    case climate::CLIMATE_ACTION_DRYING:
      return "drying";
    case climate::CLIMATE_ACTION_FAN:
      return "fan";
    case climate::CLIMATE_ACTION_DEFROSTING:
      return "defrosting";
    default:
      return nullptr;
  }
}

inline const char *ws_ha_climate_fan_mode_(climate::ClimateFanMode mode) {
  switch (mode) {
    case climate::CLIMATE_FAN_ON:
      return "on";
    case climate::CLIMATE_FAN_OFF:
      return "off";
    case climate::CLIMATE_FAN_AUTO:
      return "auto";
    case climate::CLIMATE_FAN_LOW:
      return "low";
    case climate::CLIMATE_FAN_MEDIUM:
      return "medium";
    case climate::CLIMATE_FAN_HIGH:
      return "high";
    case climate::CLIMATE_FAN_MIDDLE:
      return "middle";
    case climate::CLIMATE_FAN_FOCUS:
      return "focus";
    case climate::CLIMATE_FAN_DIFFUSE:
      return "diffuse";
    case climate::CLIMATE_FAN_QUIET:
      return "quiet";
    default:
      return nullptr;
  }
}

inline const char *ws_ha_climate_swing_mode_(climate::ClimateSwingMode mode) {
  switch (mode) {
    case climate::CLIMATE_SWING_OFF:
      return "off";
    case climate::CLIMATE_SWING_BOTH:
      return "both";
    case climate::CLIMATE_SWING_VERTICAL:
      return "vertical";
    case climate::CLIMATE_SWING_HORIZONTAL:
      return "horizontal";
    default:
      return nullptr;
  }
}

inline const char *ws_ha_climate_preset_(climate::ClimatePreset preset) {
  switch (preset) {
    case climate::CLIMATE_PRESET_NONE:
      return "none";
    case climate::CLIMATE_PRESET_HOME:
      return "home";
    case climate::CLIMATE_PRESET_AWAY:
      return "away";
    case climate::CLIMATE_PRESET_BOOST:
      return "boost";
    case climate::CLIMATE_PRESET_COMFORT:
      return "comfort";
    case climate::CLIMATE_PRESET_ECO:
      return "eco";
    case climate::CLIMATE_PRESET_SLEEP:
      return "sleep";
    case climate::CLIMATE_PRESET_ACTIVITY:
      return "activity";
    default:
      return nullptr;
  }
}

inline void ws_fill_state_climate(climate::Climate &src, JsonObject value) {
  auto traits = src.get_traits();
  if (const char *mode = ws_ha_climate_mode_(src.mode))
    value["hvac_mode"] = mode;
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION)) {
    if (const char *action = ws_ha_climate_action_(src.action))
      value["hvac_action"] = action;
  }

  const bool two_point = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE) ||
                         traits.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
  if (two_point) {
    if (!std::isnan(src.target_temperature_low))
      value["target_temp_low"] = src.target_temperature_low;
    if (!std::isnan(src.target_temperature_high))
      value["target_temp_high"] = src.target_temperature_high;
  } else if (!std::isnan(src.target_temperature)) {
    value["target_temperature"] = src.target_temperature;
  }
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE) && !std::isnan(src.current_temperature))
    value["current_temperature"] = src.current_temperature;

  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY) && !std::isnan(src.target_humidity))
    value["target_humidity"] = src.target_humidity;
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY) && !std::isnan(src.current_humidity))
    value["current_humidity"] = src.current_humidity;

  if (traits.get_supports_fan_modes()) {
    if (src.has_custom_fan_mode()) {
      value["fan_mode"] = src.get_custom_fan_mode().c_str();
    } else if (src.fan_mode.has_value()) {
      if (const char *fan = ws_ha_climate_fan_mode_(*src.fan_mode))
        value["fan_mode"] = fan;
    }
  }
  if (traits.get_supports_swing_modes()) {
    if (const char *swing = ws_ha_climate_swing_mode_(src.swing_mode))
      value["swing_mode"] = swing;
  }
  if (traits.get_supports_presets() || !traits.get_supported_custom_presets().empty()) {
    if (src.has_custom_preset()) {
      value["preset_mode"] = src.get_custom_preset().c_str();
    } else if (src.preset.has_value()) {
      if (const char *preset = ws_ha_climate_preset_(*src.preset))
        value["preset_mode"] = preset;
    }
  }
}

inline void ws_send_state_climate(WsBridgeDevice *dev, climate::Climate &src) {
  dev->get_ws_bridge_parent()->send_state_object(dev->get_ws_bridge_unique_id(),
                                                 [&src](JsonObject value) { ws_fill_state_climate(src, value); });
}

inline void ws_declare_climate(WsBridgeDevice *dev, climate::Climate &src, climate::Climate *ovr,
                               const std::string &name) {
  dev->get_ws_bridge_parent()->send_entity_declare(
      dev->get_ws_bridge_unique_id(), "climate", name, dev->get_ws_bridge_device_id(),
      dev->get_ws_bridge_device_name(), [&src, ovr](JsonObject root) {
        add_common_entity_fields(root, src, ovr);
        auto traits = src.get_traits();

        JsonArray hvac_modes = root["hvac_modes"].to<JsonArray>();
        bool has_off = false;
        for (auto mode : traits.get_supported_modes()) {
          if (const char *s = ws_ha_climate_mode_(mode)) {
            hvac_modes.add(s);
            if (mode == climate::CLIMATE_MODE_OFF)
              has_off = true;
          }
        }
        if (hvac_modes.size() == 0) {
          hvac_modes.add("off");
          has_off = true;
        }

        if (traits.get_supports_fan_modes()) {
          JsonArray fan_modes = root["fan_modes"].to<JsonArray>();
          for (auto mode : traits.get_supported_fan_modes()) {
            if (const char *s = ws_ha_climate_fan_mode_(mode))
              fan_modes.add(s);
          }
          for (const char *mode : traits.get_supported_custom_fan_modes())
            fan_modes.add(mode);
        }
        if (traits.get_supports_swing_modes()) {
          JsonArray swing_modes = root["swing_modes"].to<JsonArray>();
          for (auto mode : traits.get_supported_swing_modes()) {
            if (const char *s = ws_ha_climate_swing_mode_(mode))
              swing_modes.add(s);
          }
        }
        if (traits.get_supports_presets() || !traits.get_supported_custom_presets().empty()) {
          JsonArray preset_modes = root["preset_modes"].to<JsonArray>();
          for (auto preset : traits.get_supported_presets()) {
            if (const char *s = ws_ha_climate_preset_(preset))
              preset_modes.add(s);
          }
          for (const char *preset : traits.get_supported_custom_presets())
            preset_modes.add(preset);
        }

        root["min_temp"] = traits.get_visual_min_temperature();
        root["max_temp"] = traits.get_visual_max_temperature();
        root["target_temp_step"] = traits.get_visual_target_temperature_step();
        if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY) ||
            traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY)) {
          root["min_humidity"] = traits.get_visual_min_humidity();
          root["max_humidity"] = traits.get_visual_max_humidity();
        }
        root["temperature_unit"] = "C";

        JsonArray features = root["features"].to<JsonArray>();
        const bool requires_range = traits.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
        const bool supports_range = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
        // thermostats with SUPPORTS (not REQUIRES) two-point reject single
        // target_temperature calls — don't advertise both features.
        if (!supports_range && !requires_range)
          features.add("target_temperature");
        if (supports_range || requires_range)
          features.add("target_temperature_range");
        if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY))
          features.add("target_humidity");
        if (traits.get_supports_fan_modes())
          features.add("fan_mode");
        if (traits.get_supports_presets() || !traits.get_supported_custom_presets().empty())
          features.add("preset_mode");
        if (traits.get_supports_swing_modes())
          features.add("swing_mode");
        features.add("turn_on");
        if (has_off)
          features.add("turn_off");
      });
}

inline void ws_push_state_climate(WsBridgeDevice *dev, climate::Climate &src) { ws_send_state_climate(dev, src); }

inline void ws_subscribe_climate(WsBridgeDevice *dev, climate::Climate *src) {
  src->add_on_state_callback([dev, src](climate::Climate &) { ws_send_state_climate(dev, *src); });
}

inline void ws_handle_command_climate(climate::Climate *target, const WsCommand &command) {
  if (command.action == "set_hvac_mode") {
    std::string mode;
    if (command.param_string("hvac_mode", mode))
      target->make_call().set_mode(mode).perform();
  } else if (command.action == "set_temperature") {
    auto call = target->make_call();
    float t;
    if (command.param_float("temperature", t))
      call.set_target_temperature(t);
    if (command.param_float("target_temp_low", t))
      call.set_target_temperature_low(t);
    if (command.param_float("target_temp_high", t))
      call.set_target_temperature_high(t);
    call.perform();
  } else if (command.action == "set_fan_mode") {
    std::string mode;
    if (command.param_string("fan_mode", mode))
      target->make_call().set_fan_mode(mode).perform();
  } else if (command.action == "set_swing_mode") {
    std::string mode;
    if (command.param_string("swing_mode", mode))
      target->make_call().set_swing_mode(mode).perform();
  } else if (command.action == "set_preset_mode") {
    std::string mode;
    if (command.param_string("preset_mode", mode))
      target->make_call().set_preset(mode).perform();
  } else if (command.action == "set_humidity") {
    float humidity;
    if (command.param_float("humidity", humidity))
      target->make_call().set_target_humidity(humidity).perform();
  } else if (command.action == "turn_off") {
    target->make_call().set_mode(climate::CLIMATE_MODE_OFF).perform();
  } else if (command.action == "turn_on") {
    // Copy traits first: get_traits() returns by value, so ranging over the
    // temporary's mask would dangle after the range-init expression ends.
    auto traits = target->get_traits();
    for (auto mode : traits.get_supported_modes()) {
      if (mode != climate::CLIMATE_MODE_OFF) {
        target->make_call().set_mode(mode).perform();
        break;
      }
    }
  }
}
#endif  // USE_CLIMATE

}  // namespace ws_bridge
}  // namespace esphome
