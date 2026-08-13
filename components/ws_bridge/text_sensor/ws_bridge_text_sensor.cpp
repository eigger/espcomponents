#include "ws_bridge_text_sensor.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.text_sensor";

void WsBridgeTextSensor::setup() { ws_subscribe_text_sensor(this, this); }

void WsBridgeTextSensor::dump_config() { LOG_TEXT_SENSOR("", "WS Bridge Text Sensor", this); }

void WsBridgeTextSensor::ws_bridge_declare() {
  ws_declare_text_sensor(this, *this, this, this->get_name().str());
  ws_push_state_text_sensor(this, *this);
}

}  // namespace ws_bridge
}  // namespace esphome
