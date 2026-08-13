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
#ifdef USE_FAN
#include "esphome/components/fan/fan.h"
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

}  // namespace ws_bridge
}  // namespace esphome
