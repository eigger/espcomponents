#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/text_sensor/text_sensor.h"
#define MAX_KEY 35
namespace esphome {
namespace lilygo_t_keyboard {

class LilygoTKeyboard : public Component {
public:
    LilygoTKeyboard() = default;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::HARDWARE; }
    void dump_config() override;
    void set_key(text_sensor::TextSensor *key) { key_ = key; }
    void set_last_key(text_sensor::TextSensor *key) { last_key_ = key; }
protected:

    void readKey();
    void on_press_key(int key);
    void on_release_key(int key);
    text_sensor::TextSensor *key_{nullptr};
    text_sensor::TextSensor *last_key_{nullptr};
    std::vector<byte> rows_{0, 3, 18, 12, 11, 6, 7};
    std::vector<byte> cols_{1, 4, 5, 19, 13};
    bool lastPressedKey_[MAX_KEY]();
};

}  // namespace lilygo_t_keyboard
}  // namespace esphome
