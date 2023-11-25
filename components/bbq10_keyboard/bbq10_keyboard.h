#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace bbq10_keyboard {

class BBQ10Keyboard : public Component, public i2c::I2CDevice {
public:
    BBQ10Keyboard() = default;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::HARDWARE; }
    void dump_config() override;
    void set_key(text_sensor::TextSensor *key) { key_ = key; }
protected:
    // read a given register
    bool read_reg_(uint8_t reg, uint8_t *value);
    bool read_reg16_(uint8_t reg, uint16_t *value);
    // write a value to a given register
    bool write_reg_(uint8_t reg, uint8_t value);
    std::string key_string(uint8_t key);
    text_sensor::TextSensor *key_{nullptr};
    uint8_t keyValue_{0};
};

}  // namespace bbq10_keyboard
}  // namespace esphome
