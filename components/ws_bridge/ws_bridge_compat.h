#pragma once
#include <string>
#include "esphome/core/entity_base.h"
#include "esphome/core/version.h"

namespace esphome {
namespace ws_bridge {

// Entity name as std::string. ESPHome 2026.11 deprecated EntityBase::get_name()
// (removed in 2027.5) in favour of get_name_to(); older releases only have get_name().
inline std::string ws_entity_name(const EntityBase &e) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
  char buf[ENTITY_NAME_BUF_SIZE];
  return e.get_name_to(buf).str();
#else
  return e.get_name().str();
#endif
}

}  // namespace ws_bridge
}  // namespace esphome
