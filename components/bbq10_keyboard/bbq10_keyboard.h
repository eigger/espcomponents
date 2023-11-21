#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace bbq10_keyboard {

#define _REG_VER 0x01 // fw version
#define _REG_CFG 0x02 // config
#define _REG_INT 0x03 // interrupt status
#define _REG_KEY 0x04 // key status
#define _REG_BKL 0x05 // backlight
#define _REG_DEB 0x06 // debounce cfg
#define _REG_FRQ 0x07 // poll freq cfg
#define _REG_RST 0x08 // reset
#define _REG_FIF 0x09 // fifo
#define _REG_BK2 0x0A // backlight 2
#define _REG_DIR 0x0B // gpio direction
#define _REG_PUE 0x0C // gpio input pull enable
#define _REG_PUD 0x0D // gpio input pull direction
#define _REG_GIO 0x0E // gpio value
#define _REG_GIC 0x0F // gpio interrupt config
#define _REG_GIN 0x10 // gpio interrupt status

#define CFG_OVERFLOW_ON  (1 << 0)
#define CFG_OVERFLOW_INT (1 << 1)
#define CFG_CAPSLOCK_INT (1 << 2)
#define CFG_NUMLOCK_INT  (1 << 3)
#define CFG_KEY_INT      (1 << 4)
#define CFG_PANIC_INT    (1 << 5)

#define INT_OVERFLOW     (1 << 0)
#define INT_CAPSLOCK     (1 << 1)
#define INT_NUMLOCK      (1 << 2)
#define INT_KEY          (1 << 3)
#define INT_PANIC        (1 << 4)

#define KEY_CAPSLOCK     (1 << 5)
#define KEY_NUMLOCK      (1 << 6)
#define KEY_COUNT_MASK   (0x1F)

#define DIR_OUTPUT       0
#define DIR_INPUT        1

#define PUD_DOWN         0
#define PUD_UP           1

enum KeyState
{
    StateIdle = 0,
    StatePress,
    StateLongPress,
    StateRelease
};

struct KeyEvent
{
    char key;
    KeyState state;
};

class BBQ10Keyboard : public Component, public i2c::I2CDevice {
public:
    BBQ10Keyboard() = default;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::HARDWARE; }
    void dump_config() override;

    void set_key(text_sensor::TextSensor *key) { key_ = key; }
    void set_key_state(text_sensor::TextSensor *state) { keyState_ = state; }
    void set_brightness(number::Number *brightness) { brightness_ = brightness; }
    void set_backlight(float value);
    void brightness_callback(float value);
protected:
    // read a given register
    bool read_reg_(uint8_t reg, uint8_t *value);
    bool read_reg16_(uint8_t reg, uint16_t *value);
    // write a value to a given register
    bool write_reg_(uint8_t reg, uint8_t value);
    std::string key_string(char key);
    std::string key_state_string(KeyState state);

    text_sensor::TextSensor *key_{nullptr};
    text_sensor::TextSensor *keyState_{nullptr};
    number::Number *brightness_{nullptr};
    KeyEvent oldEvent_{ .key = '\0', .state = StateIdle };
};

class Brightness : public number::Number
{
public:
    void control(float value)
    {
        this->state = value;
        this->publish_state(value);
    }
};

}  // namespace bbq10_keyboard
}  // namespace esphome
