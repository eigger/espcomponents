#include "bbq10_keyboard.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bbq10_keyboard {

static const char *const TAG = "bbq10_keyboard";

void BBQ10Keyboard::setup()
{
    ESP_LOGCONFIG(TAG, "Setting up BBQ10Keyboard...");
    if (this->key_)
    {
        this->key_->publish_state("");
    }
}

void BBQ10Keyboard::dump_config()
{
    ESP_LOGCONFIG(TAG, "BBQ10Keyboard");
}

void BBQ10Keyboard::loop()
{
    uint8_t value = 0;
    read_reg_(0, &value);
    if (value != keyValue_)
    {
        keyValue_ = value;
        if (this->key_)
        {
            ESP_LOGI(TAG, "Key: 0x%x", value);
            this->key_->publish_state(key_string(value));
        }
    }
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

std::string BBQ10Keyboard::key_string(uint8_t key)
{
    return std::string(1, key);
}

}  // namespace bbq10_keyboard
}  // namespace esphome
