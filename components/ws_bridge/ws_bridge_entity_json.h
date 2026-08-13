#pragma once
#include <array>
#include "esphome/core/entity_base.h"
#include "ws_protocol.h"

namespace esphome {
namespace ws_bridge {

// Adds the declare fields common to every ws_bridge entity platform
// (device_class, icon, entity_category) from the entity's own EntityBase
// metadata. Platform-specific fields (unit_of_measurement/state_class for
// sensor, options for select, min/max/step for number, ...) are added by each
// platform's own ws_bridge_declare() on top of this.
//
// `src` is where the metadata comes from — the wrapped entity when this
// ws_bridge entity wraps a foreign one, otherwise the ws_bridge entity itself.
// `ovr` (may be null) is the ws_bridge-side entity carrying whatever the YAML
// set explicitly: each field is taken from `ovr` when set there and falls back
// to `src` otherwise. So wrapping inherits the wrapped entity's presentation
// for free, while `icon:`/`device_class:`/`entity_category:` written on the
// ws_bridge platform still win.
//
// Every field here has an unambiguous "unset" value (empty string, or
// ENTITY_CATEGORY_NONE), so the fallback needs no extra flags. Fields where
// the default is a legitimate value — sensor's accuracy_decimals, whose 0 is
// indistinguishable from "not written" — cannot be resolved this way and need
// codegen to say explicitly whether to inherit.
inline void add_common_entity_fields(JsonObject root, const EntityBase &src, const EntityBase *ovr) {
  std::array<char, MAX_DEVICE_CLASS_LENGTH> dc_ovr_buf;
  std::array<char, MAX_DEVICE_CLASS_LENGTH> dc_src_buf;
  const char *dc = ovr != nullptr ? ovr->get_device_class_to(dc_ovr_buf) : nullptr;
  if (dc == nullptr || dc[0] == '\0') dc = src.get_device_class_to(dc_src_buf);
  if (dc != nullptr && dc[0] != '\0') root["device_class"] = dc;

  std::array<char, MAX_ICON_LENGTH> icon_ovr_buf;
  std::array<char, MAX_ICON_LENGTH> icon_src_buf;
  const char *icon = ovr != nullptr ? ovr->get_icon_to(icon_ovr_buf) : nullptr;
  if (icon == nullptr || icon[0] == '\0') icon = src.get_icon_to(icon_src_buf);
  if (icon != nullptr && icon[0] != '\0') root["icon"] = icon;

  EntityCategory category = ovr != nullptr ? ovr->get_entity_category() : ENTITY_CATEGORY_NONE;
  if (category == ENTITY_CATEGORY_NONE) category = src.get_entity_category();
  switch (category) {
    case ENTITY_CATEGORY_CONFIG:
      root["entity_category"] = "config";
      break;
    case ENTITY_CATEGORY_DIAGNOSTIC:
      root["entity_category"] = "diagnostic";
      break;
    default:
      break;
  }
}

}  // namespace ws_bridge
}  // namespace esphome
