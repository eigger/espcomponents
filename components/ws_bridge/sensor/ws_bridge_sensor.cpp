#include "ws_bridge_sensor.h"
#include "../ws_bridge.h"
#include "../ws_bridge_entity_json.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.sensor";

void WsBridgeSensor::setup() {
  this->add_on_state_callback([this](float state) { this->parent_->send_state_float(this->unique_id_, state); });
}

void WsBridgeSensor::dump_config() { LOG_SENSOR("", "WS Bridge Sensor", this); }

void WsBridgeSensor::ws_bridge_declare() {
  this->parent_->send_entity_declare(
      this->unique_id_, "sensor", this->get_name().str(), this->device_id_, this->device_name_,
      [this](JsonObject root) {
        add_common_entity_fields(root, *this);
        StringRef uom = this->get_unit_of_measurement_ref();
        if (!uom.empty()) root["unit_of_measurement"] = uom.str();
        sensor::StateClass sc = this->get_state_class();
        if (sc != sensor::STATE_CLASS_NONE) root["state_class"] = LOG_STR_ARG(sensor::state_class_to_string(sc));
        root["suggested_display_precision"] = this->get_accuracy_decimals();
      });
  if (this->has_state()) this->parent_->send_state_float(this->unique_id_, this->state);
}

}  // namespace ws_bridge
}  // namespace esphome
