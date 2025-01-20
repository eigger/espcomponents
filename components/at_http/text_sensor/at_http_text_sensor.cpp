#include "at_http_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace at_http {

static const char *TAG = "at_http.text_sensor";

void ATHttpTextSensor::dump_config() 
{
    ESP_LOGCONFIG(TAG, "AT Http Text Sensor '%s':", get_name().c_str());
    uartex_dump_config(TAG);
}

void ATHttpTextSensor::update()
{
    enqueue_tx_cmd(get_command("http_request"), false);
}

void ATHttpTextSensor::setup()
{
    std::string str;
    if (!http_payload_.empty()) {
        str = "AT+HTTPCLIENT=" + std::to_string(http_method_) + "," +
              std::to_string(http_content_) + ",\"" + http_url_ +
              "\",\"\",\"\",1,\"" + http_payload_ + "\"";
    } else {
        str = "AT+HTTPCLIENT=" + std::to_string(http_method_) + "," +
              std::to_string(http_content_) + ",\"" + http_url_ + "\",\"\",\"\",1";
    }
    http_request_.data = std::vector<uint8_t>(str.begin(), str.end());
    set_command("http_request", http_request_);
}

void ATHttpTextSensor::publish(const std::vector<uint8_t>& data) 
{
    optional<const char*> val = get_state_str("lambda", data);
    if(val.has_value() && this->raw_state != val.value())
    {
        this->raw_state = val.value();
        publish_state(val.value());
    }
}

}  // namespace at_http
}  // namespace esphome
