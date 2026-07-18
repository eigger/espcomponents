#include "ws_bridge_binary_sensor.h"
#include "../ws_bridge.h"
#include "../ws_bridge_entity_json.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.binary_sensor";

void WsBridgeBinarySensor::setup() {
  this->add_on_state_callback([this](bool state) { this->parent_->send_state_bool(this->unique_id_, state); });
}

void WsBridgeBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "WS Bridge Binary Sensor", this); }

void WsBridgeBinarySensor::ws_bridge_declare() {
  this->parent_->send_entity_declare(this->unique_id_, "binary_sensor", this->get_name().str(), this->device_id_,
                                     this->device_name_,
                                     [this](JsonObject root) { add_common_entity_fields(root, *this); });
  if (this->has_state()) this->parent_->send_state_bool(this->unique_id_, this->state);
}

}  // namespace ws_bridge
}  // namespace esphome
