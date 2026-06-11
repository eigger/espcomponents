#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace tca8418_keyboard {

enum Model {
    MODEL_CARDPUTER_ADV = 0,
};

// TCA8418 keypad scan IC. The chip handles matrix scanning and debounce in
// hardware and queues press/release events in a FIFO; we only drain that FIFO.
// The selected model defines the matrix size, the scan->layout remap and the
// key map.
class TCA8418Keyboard : public Component, public i2c::I2CDevice {
public:
    static const int ROWS = 4;
    static const int COLS = 14;

    TCA8418Keyboard() = default;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::HARDWARE; }
    void dump_config() override;
    void set_press_key(text_sensor::TextSensor *key) { press_key_ = key; }
    void set_release_key(text_sensor::TextSensor *key) { release_key_ = key; }
    void set_irq_pin(GPIOPin *pin) { irq_pin_ = pin; }
    void set_model(Model model) { model_ = model; }
    void add_on_key_press_callback(std::function<void(std::string)> &&cb) { this->press_callback_.add(std::move(cb)); }
    void add_on_key_release_callback(std::function<void(std::string)> &&cb) { this->release_callback_.add(std::move(cb)); }
protected:
    bool read_reg_(uint8_t reg, uint8_t *value);
    bool write_reg_(uint8_t reg, uint8_t value);
    void flush_events_();
    void process_event_(uint8_t event);
    bool remap_(uint8_t raw_row, uint8_t raw_col, int *row, int *col);
    void on_press_key_(int row, int col);
    void on_release_key_(int row, int col);
    bool is_modifier_(int row, int col);
    std::string key_to_string_(int row, int col);

    text_sensor::TextSensor *press_key_{nullptr};
    text_sensor::TextSensor *release_key_{nullptr};
    GPIOPin *irq_pin_{nullptr};
    Model model_{MODEL_CARDPUTER_ADV};
    bool pressed_[ROWS][COLS]{};
    CallbackManager<void(std::string)> press_callback_{};
    CallbackManager<void(std::string)> release_callback_{};
};

class KeyPressTrigger : public Trigger<std::string> {
public:
    explicit KeyPressTrigger(TCA8418Keyboard *parent) {
        parent->add_on_key_press_callback([this](std::string key) { this->trigger(std::move(key)); });
    }
};

class KeyReleaseTrigger : public Trigger<std::string> {
public:
    explicit KeyReleaseTrigger(TCA8418Keyboard *parent) {
        parent->add_on_key_release_callback([this](std::string key) { this->trigger(std::move(key)); });
    }
};

}  // namespace tca8418_keyboard
}  // namespace esphome
