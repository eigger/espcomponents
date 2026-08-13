#include "ws_bridge_text_sensor.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.text_sensor";

void WsBridgeTextSensor::setup() {
  ws_subscribe_text_sensor(this, this->source_ != nullptr ? this->source_ : this);
}

void WsBridgeTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "WS Bridge Text Sensor", this);
  if (this->source_ != nullptr) ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

void WsBridgeTextSensor::ws_bridge_declare() {
  text_sensor::TextSensor &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_text_sensor(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_text_sensor(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
