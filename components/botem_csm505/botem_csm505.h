#pragma once
#include <vector>
#include <queue>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "parser.h"
#include "version.h"
namespace esphome {
namespace botem_csm505 {

class BotemCSM505Component : public uart::UARTDevice, public number::Number, public Component
{
public:
    BotemCSM505Component() = default;
    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }

    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
    void control(float value) override;
    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_last_error(text_sensor::TextSensor *last_error) { last_error_ = last_error; }
    //void set_people_count(number::Number *people_count) { people_count_ = people_count; }
protected:

    uint16_t conf_rx_timeout_{10};
    bool validate_data();
    void read_from_uart();
    void publish_data();

    Parser rx_parser_{};
    text_sensor::TextSensor *version_{nullptr};
    text_sensor::TextSensor *last_error_{nullptr};
    //number::Number *people_count_{nullptr};
};

} // namespace botem_csm505
} // namespace esphome