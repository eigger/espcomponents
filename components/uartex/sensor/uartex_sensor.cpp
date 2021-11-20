#include "uartex_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.sensor";

void UartExSensor::dump_config() {
    ESP_LOGCONFIG(TAG, "UartEx Sensor '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

void UartExSensor::publish(const uint8_t *data, const num_t len) {
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

}  // namespace uartex
}  // namespace esphome
