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
inline void add_common_entity_fields(JsonObject root, const EntityBase &entity) {
  std::array<char, MAX_DEVICE_CLASS_LENGTH> dc_buf;
  const char *dc = entity.get_device_class_to(dc_buf);
  if (dc != nullptr && dc[0] != '\0') root["device_class"] = dc;

  std::array<char, MAX_ICON_LENGTH> icon_buf;
  const char *icon = entity.get_icon_to(icon_buf);
  if (icon != nullptr && icon[0] != '\0') root["icon"] = icon;

  switch (entity.get_entity_category()) {
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
