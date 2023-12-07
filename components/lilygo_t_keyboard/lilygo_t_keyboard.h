#pragma once
#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/text_sensor/text_sensor.h"
#define MAX_KEY 35

// q w e r t y u i o p
// 0 1 7 14 16 23 21 30 28 10
// a s d f g h j k l BS
// 3 8 9 20 15 22 27 34 29 31
// alt z x c v b n m $ enter
// 4 12 11 19 18 25 26 33 32 24
// lshi mic space sym rshi
// 13 6 5 2 17
enum KEY {
    KEY_Q,  //0
    KEY_W,  //1
    KEY_SYM,  //2
    KEY_A,  //3
    KEY_ALT,  //4
    KEY_SPACE,  //5
    KEY_MIC,  //6
    KEY_E,  //7
    KEY_S,  //8
    KEY_D,  //9
    KEY_P,  //10
    KEY_X,  //11
    KEY_Z,  //12
    KEY_LSHIFT,  //13
    KEY_R,  //14
    KEY_G,  //15
    KEY_T,  //16
    KEY_RSHIFT,  //17
    KEY_V,  //18
    KEY_C,  //19
    KEY_F,  //20
    KEY_U,  //21
    KEY_H,  //22
    KEY_Y,  //23
    KEY_ENTER,  //24
    KEY_B,  //25
    KEY_N,  //26
    KEY_J,  //27
    KEY_O,  //28
    KEY_L,  //29
    KEY_I,  //30
    KEY_BS,  //31
    KEY_DOLLAR,  //32
    KEY_M,  //33
    KEY_K,  //34
};

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
    std::string key_to_string(int key);
    bool contains_key(int key);
    void add_key(int key);
    void remove_key(int key);
    void publish_key();
    text_sensor::TextSensor *key_{nullptr};
    text_sensor::TextSensor *last_key_{nullptr};
    std::vector<int> rows_{0, 3, 18, 12, 11, 6, 7};
    std::vector<int> cols_{1, 4, 5, 19, 13};
    std::vector<int> pressedKey_;
    bool lastPressedKey_[MAX_KEY];
};

}  // namespace lilygo_t_keyboard
}  // namespace esphome
