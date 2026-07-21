#include "ws_bridge_text_sensor.h"
#include "../ws_bridge.h"
#include "../ws_bridge_entity_json.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.text_sensor";

void WsBridgeTextSensor::setup() {
  this->add_on_state_callback(
      [this](const std::string &state) { this->parent_->send_state_string(this->unique_id_, state); });
}

void WsBridgeTextSensor::dump_config() { LOG_TEXT_SENSOR("", "WS Bridge Text Sensor", this); }

void WsBridgeTextSensor::ws_bridge_declare() {
  this->parent_->send_entity_declare(this->unique_id_, "text_sensor", this->get_name().str(), this->device_id_,
                                     this->device_name_,
                                     [this](JsonObject root) { add_common_entity_fields(root, *this); });
  if (this->has_state()) this->parent_->send_state_string(this->unique_id_, this->state);
}

}  // namespace ws_bridge
}  // namespace esphome
