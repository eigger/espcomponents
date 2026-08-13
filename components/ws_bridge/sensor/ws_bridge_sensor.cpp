#include "ws_bridge_sensor.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.sensor";

void WsBridgeSensor::setup() { ws_subscribe_sensor(this, this); }

void WsBridgeSensor::dump_config() { LOG_SENSOR("", "WS Bridge Sensor", this); }

void WsBridgeSensor::ws_bridge_declare() {
  ws_declare_sensor(this, *this, this, this->get_name().str());
  ws_push_state_sensor(this, *this);
}

}  // namespace ws_bridge
}  // namespace esphome
