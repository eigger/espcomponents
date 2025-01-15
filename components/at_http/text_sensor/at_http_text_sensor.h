#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uartex/uartex_device.h"

namespace esphome {
namespace at_http {

enum HTTP_METHOD {
    HEAD = 1, 
    GET, 
    POST, 
    PUT, 
    DELETE
};

enum HTTP_CONTENT {
    APPLICATION_X_WWW_FORM_URLENCODED = 0,
    APPLICATION_JSON,
    MULTIPART_FORM_DATA,
    TEXT_XML
};


class ATHttpTextSensor : public text_sensor::TextSensor, public UARTExDevice
{
public:
    void dump_config() override;
    void update() override;
    void set_http_method(HTTP_METHOD method) { http_method_ = method; }
    void set_http_content(HTTP_CONTENT content) { http_content_ = content; }
    void set_http_url( std::string url) { http_url_ = url; }
    void set_http_payload( std::string payload) { http_payload_ = payload; }
protected:
    void setup() override;
    void publish(const std::vector<uint8_t>& data) override;
    
protected:
    HTTP_METHOD http_method_{HEAD};
    HTTP_CONTENT http_content_{APPLICATION_X_WWW_FORM_URLENCODED};
    std::string http_url_{""};
    std::string http_payload_{""};
    cmd_t http_request_{};
};

}  // namespace at_http
}  // namespace esphome
