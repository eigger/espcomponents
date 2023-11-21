#include "bbq10_keyboard.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bbq10_keyboard {

static const char *const TAG = "bbq10_keyboard";

void BBQ10Keyboard::setup()
{
    ESP_LOGCONFIG(TAG, "Setting up BBQ10Keyboard...");
    reset();
    if (this->brightness_)
    {
        this->brightness_->add_on_state_callback(std::bind(&BBQ10Keyboard::brightness_callback, this, std::placeholders::_1));
        this->brightness_->publish_state(50);
    }
    if (this->key_)
    {
        this->key_->publish_state("");
    }
    if (this->keyState_)
    {
        this->keyState_->publish_state("");
    }
    if (this->pressedKey_)
    {
        this->pressedKey_->publish_state("");
    }
}

void BBQ10Keyboard::dump_config()
{
    ESP_LOGCONFIG(TAG, "BBQ10Keyboard");
}

void BBQ10Keyboard::loop()
{
    if (key_count() == 0) return;
    uint16_t value = key_value();
    if (value != keyValue_)
    {
        KeyEvent event = key_event(value);
        update_key_map(event);
        if (this->key_)
        {
            this->key_->publish_state(key_string(event.key));
        }
        if (this->keyState_)
        {
            this->keyState_->publish_state(key_state_string(event.state));
        }
        if (this->pressedKey_)
        {
            this->pressedKey_->publish_state(key_map_string(keyMap_));
        }
    }
    keyValue_ = value;
}

void BBQ10Keyboard::reset()
{
    uint8_t data[1] = { _REG_RST };
    this->write(data, 1);
}

uint8_t BBQ10Keyboard::key_count()
{
    uint8_t value = 0;
    read_reg_(_REG_KEY, &value);
    return value & KEY_COUNT_MASK;
}

KeyEvent BBQ10Keyboard::key_event(uint16_t value)
{
    KeyEvent event = { .key = '\0', .state = StateIdle };
    event.key = value >> 8;
    event.state = KeyState(value & 0xFF);
    return event;
}

uint16_t BBQ10Keyboard::key_value()
{
    uint16_t value = 0;
    read_reg16_(_REG_FIF, &value);
    return value;
}

float BBQ10Keyboard::backlight()
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

std::string BBQ10Keyboard::key_map_string(std::unordered_map<char, KeyState> map)
{
    std::string str;
    for (const auto& pair : map)
    {
        if (str.size() > 0) str += "+";
        str += key_string(pair.first);
    }
    return str;
}

void BBQ10Keyboard::update_key_map(KeyEvent event)
{
    if (event.state == StatePress || event.state == StateLongPress)
    {
        keyMap_[event.key] = event.state;
    }
    else
    {
        keyMap_.erase(event.key);
    }
}

}  // namespace bbq10_keyboard
}  // namespace esphome
