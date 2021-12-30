#include "rs485_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rs485 {

static const char *TAG = "rs485.sensor";

void RS485Sensor::dump_config() {
    ESP_LOGCONFIG(TAG, "RS485 Sensor '%s':", device_name_->c_str());
    dump_rs485_device_config(TAG);
}

void RS485Sensor::publish(const uint8_t *data, const num_t len) {
    if (this->f_.has_value()) {
        optional<float> val = (*this->f_)(data, len);
        if(val.has_value() && this->raw_state != val.value())
            this->publish_state(val.value());
    }
    else if(this->conf_state_num_.has_value() && len >= (this->conf_state_num_.value().offset + this->conf_state_num_.value().length)) {
        float val = hex_to_float(&data[this->conf_state_num_.value().offset], this->conf_state_num_.value().length, this->conf_state_num_.value().precision);
        if(this->raw_state != val)
            this->publish_state(val);
    }
}

}  // namespace rs485
}  // namespace esphome
