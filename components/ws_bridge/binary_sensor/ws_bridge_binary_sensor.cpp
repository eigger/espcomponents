#include "ws_bridge_binary_sensor.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.binary_sensor";

void WsBridgeBinarySensor::setup() { ws_subscribe_binary_sensor(this, this); }

void WsBridgeBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "WS Bridge Binary Sensor", this); }

void WsBridgeBinarySensor::ws_bridge_declare() {
  ws_declare_binary_sensor(this, *this, this, this->get_name().str());
  ws_push_state_binary_sensor(this, *this);
}

}  // namespace ws_bridge
}  // namespace esphome
