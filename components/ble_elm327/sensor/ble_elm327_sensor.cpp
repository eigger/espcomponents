#include "ble_elm327_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_elm327 {

static const char *const TAG = "ble_elm327.sensor";

void BleElm327Sensor::dump_config() {
  LOG_SENSOR("  ", "BLE ELM327 Sensor", this);
  ESP_LOGCONFIG(TAG, "    Mode: %s  PID: %s  Response size: %d",
                mode_.c_str(), pid_.c_str(), response_size_);
}

void BleElm327Sensor::publish_data(const std::vector<uint8_t> &data) {
  this->publish_state(this->parse_float(data));
}

}  // namespace ble_elm327
}  // namespace esphome
