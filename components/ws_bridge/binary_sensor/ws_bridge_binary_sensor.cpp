#include "ws_bridge_binary_sensor.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.binary_sensor";

void WsBridgeBinarySensor::setup() {
  ws_subscribe_binary_sensor(this, this->source_ != nullptr ? this->source_ : this);
}

void WsBridgeBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "WS Bridge Binary Sensor", this);
  if (this->source_ != nullptr) ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

void WsBridgeBinarySensor::ws_bridge_declare() {
  binary_sensor::BinarySensor &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_binary_sensor(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_binary_sensor(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
