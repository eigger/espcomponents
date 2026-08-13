#include "ws_bridge_sensor.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.sensor";

void WsBridgeSensor::setup() { ws_subscribe_sensor(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeSensor::dump_config() {
  LOG_SENSOR("", "WS Bridge Sensor", this);
  if (this->source_ != nullptr) ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

void WsBridgeSensor::ws_bridge_declare() {
  sensor::Sensor &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_sensor(this, src, this, ws_ha_name(src, own_name, this->unique_id_),
                    this->accuracy_overridden_);
  ws_push_state_sensor(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
