#include "rs485_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rs485 {

static const char *TAG = "rs485.switch";

void RS485Switch::dump_config() {
    ESP_LOGCONFIG(TAG, "RS485 Switch '%s':", device_name_->c_str());
    dump_rs485_device_config(TAG);
}

void RS485Switch::publish(const uint8_t *data, const num_t len) {
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(&data[0], len).c_str());
}

}  // namespace rs485
}  // namespace esphome
