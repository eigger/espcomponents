#include "bbq10_keyboard.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bbq10_keyboard {

static const char *const TAG = "bbq10_keyboard";

void BBQ10Keyboard::setup()
{
    ESP_LOGCONFIG(TAG, "Setting up BBQ10Keyboard...");
    this->write_command(_REG_RST);
    if (this->brightness_)
    {
        this->brightness_->add_on_state_callback(std::bind(&BBQ10Keyboard::brightness_callback, this, std::placeholders::_1));
        this->brightness_->publish_state(50);
    }
    if (this->key_)
    {
        this->key_->publish_state(key_string(oldEvent_.key));
    }
    if (this->keyState_)
    {
        this->keyState_->publish_state(key_state_string(oldEvent_.key));
    }
}

void BBQ10Keyboard::dump_config()
{
    ESP_LOGCONFIG(TAG, "BBQ10Keyboard");
}

void BBQ10Keyboard::loop()
{
    if (keyCount() == 0) return;
    const BBQ10Keyboard::KeyEvent event = keyEvent();
    if (event.key != oldEvent_.key || event.state != oldEvent_.state)
    {
        if (this->key_)
        {
            this->key_->publish_state(key_string(event.key));
        }
        if (this->keyState_)
        {
            this->keyState_->publish_state(key_state_string(event.key));
        }
    }
    oldEvent_ = event;
}

uint8_t BBQ10Keyboard::status() const
{
    uint8_t value = 0;
    read_reg_(_REG_KEY, &value);
    return value;
}

uint8_t BBQ10Keyboard::keyCount() const
{
    return status() & KEY_COUNT_MASK;
}

BBQ10Keyboard::KeyEvent BBQ10Keyboard::keyEvent() const
{
    KeyEvent event = { .key = '\0', .state = StateIdle };
    if (keyCount() == 0) return event;
    uint16_t value = 0;
    read_reg16_(_REG_FIF, &value);
    event.key = value >> 8;
    event.state = KeyState(value & 0xFF);
    return event;
}

float BBQ10Keyboard::backlight() const
{
    uint8_t value = 0;
    read_reg_(_REG_BKL, &value);
    return value > 0 ? value / 255.0f : 0;
}

void BBQ10Keyboard::set_backlight(float value)
{
    this->write_reg_(_REG_BKL, value * 255);
}

bool BBQ10Keyboard::read_reg_(uint8_t reg, uint8_t *value)
{
    if (this->is_failed()) return false;
    return this->read_byte(reg, value);
}

bool BBQ10Keyboard::read_reg16_(uint8_t reg, uint16_t *value)
{
    if (this->is_failed()) return false;
    return this->read_byte_16(reg, value);
}

bool BBQ10Keyboard::write_reg_(uint8_t reg, uint8_t value)
{
    if (this->is_failed()) return false;
    return this->write_byte(reg, value);
}

void BBQ10Keyboard::brightness_callback(float value)
{
    set_backlight(value / 100.0f);
}

std::string BBQ10Keyboard::key_string(char key)
{
    return std::string(1, key);
}

std::string BBQ10Keyboard::key_state_string(KeyState state)
{
    switch(state)
    {
    case KeyState::StatePress:
        return "Press"; 
    case KeyState::StateLongPress:
        return "LongPress";
    case KeyState::StateRelease:
        return "Release";
    }
    return "Idle"; 
}

}  // namespace bbq10_keyboard
}  // namespace esphome
