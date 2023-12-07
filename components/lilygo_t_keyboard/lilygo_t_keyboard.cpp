#include "lilygo_t_keyboard.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lilygo_t_keyboard {

static const char *const TAG = "lilygo_t_keyboard";

void LilygoTKeyboard::setup()
{
    for (int row : rows_)
    {
        pinMode(row, INPUT);
    }
    for (int col : cols_)
    {
        pinMode(col, INPUT_PULLUP);
    }
    for (int i = 0 ; i < MAX_KEY; i++)
    {
        lastPressedKey_[i] = false;
    }
    if (key_ != nullptr) key_->publish_state("None");
    if (last_key_ != nullptr) last_key_->publish_state("None");
    ESP_LOGCONFIG(TAG, "Setting up LilygoTKeyboard...");
}

void LilygoTKeyboard::dump_config()
{
    ESP_LOGCONFIG(TAG, "LilygoTKeyboard");
}

void LilygoTKeyboard::loop()
{
    readKey();
}

void LilygoTKeyboard::readKey()
{
    for (size_t colIndex = 0; colIndex < cols_.size(); colIndex++)
    {
        int curCol = cols_[colIndex];
        pinMode(curCol, OUTPUT);
        digitalWrite(curCol, LOW);
        for (size_t rowIndex = 0; rowIndex < rows_.size(); rowIndex++)
        {
            int rowCol = rows_[rowIndex];
            pinMode(rowCol, INPUT_PULLUP);
            delay(1);
            bool keyPressed = (digitalRead(rowCol) == LOW);
            int key = colIndex * rows_.size() + rowIndex;
            if (lastPressedKey_[key] != keyPressed)
            {
                lastPressedKey_[key] = keyPressed;
                if (keyPressed) on_press_key(key);
                else            on_release_key(key);
            }
            pinMode(rowCol, INPUT);
        }
        pinMode(curCol, INPUT);
    }
}

void LilygoTKeyboard::on_press_key(int key)
{
    ESP_LOGD(TAG, "press key (%d)", key);
    if (key_ != nullptr) key_->publish_state(key_to_string(key));
    if (last_key_ != nullptr) last_key_->publish_state(key_to_string(key));
}

void LilygoTKeyboard::on_release_key(int key)
{
    ESP_LOGD(TAG, "release key (%d)", key);
    if (key_ != nullptr) key_->publish_state("None");    
}

std::string LilygoTKeyboard::key_to_string(int key)
{
    switch(key)
    {
    case KEY_Q:  //0
        if (lastPressedKey_[KEY_SYM]) return "#";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "Q" : "q";
    case KEY_W:  //1
        if (lastPressedKey_[KEY_SYM]) return "1";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "W" : "w";
    case KEY_SYM:  //2
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "SYM" : "sym";
    case KEY_A:  //3
        if (lastPressedKey_[KEY_SYM]) return "*";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "A" : "a";
    case KEY_ALT:  //4
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "ALT" : "alt";
    case KEY_SPACE:  //5
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "SPACE" : "space";
    case KEY_MIC:  //6
        if (lastPressedKey_[KEY_SYM]) return "0";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "MIC" : "mic";
    case KEY_E:  //7
        if (lastPressedKey_[KEY_SYM]) return "2";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "E" : "e";
    case KEY_S:  //8
        if (lastPressedKey_[KEY_SYM]) return "4";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "S" : "s";
    case KEY_D:  //9
        if (lastPressedKey_[KEY_SYM]) return "5";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "D" : "d";
    case KEY_P:  //10
        if (lastPressedKey_[KEY_SYM]) return "@";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "P" : "p";
    case KEY_X:  //11
        if (lastPressedKey_[KEY_SYM]) return "8";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "X" : "x";
    case KEY_Z:  //12
        if (lastPressedKey_[KEY_SYM]) return "7";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "Z" : "z";
    case KEY_LSHIFT:  //13
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "LSHIFT" : "lshift";
    case KEY_R:  //14
        if (lastPressedKey_[KEY_SYM]) return "3";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "R" : "r";
    case KEY_G:  //15
        if (lastPressedKey_[KEY_SYM]) return "/";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "G" : "g";
    case KEY_T:  //16
        if (lastPressedKey_[KEY_SYM]) return "()";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "T" : "t";
    case KEY_RSHIFT:  //17
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "RSHIFT" : "rshift";
    case KEY_V:  //18
        if (lastPressedKey_[KEY_SYM]) return "?";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "V" : "v";
    case KEY_C:  //19
        if (lastPressedKey_[KEY_SYM]) return "9";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "C" : "c";
    case KEY_F:  //20
        if (lastPressedKey_[KEY_SYM]) return "6";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "F" : "f";
    case KEY_U:  //21
        if (lastPressedKey_[KEY_SYM]) return "_";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "U" : "u";
    case KEY_H:  //22
        if (lastPressedKey_[KEY_SYM]) return ":";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "H" : "h";
    case KEY_Y:  //23
        if (lastPressedKey_[KEY_SYM]) return ")";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "Y" : "y";
    case KEY_ENTER:  //24
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "ENTER" : "enter";
    case KEY_B:  //25
        if (lastPressedKey_[KEY_SYM]) return "!";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "B" : "b";
    case KEY_N:  //26
        if (lastPressedKey_[KEY_SYM]) return ",";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "N" : "n";
    case KEY_J:  //27
        if (lastPressedKey_[KEY_SYM]) return ";";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "J" : "j";
    case KEY_O:  //28
        if (lastPressedKey_[KEY_SYM]) return "+";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "O" : "o";
    case KEY_L:  //29
        if (lastPressedKey_[KEY_SYM]) return "\"";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "L" : "l";
    case KEY_I:  //30
        if (lastPressedKey_[KEY_SYM]) return "-";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "I" : "i";
    case KEY_BS:  //31
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "BS" : "bs";
    case KEY_DOLLAR:  //32
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "DOLLAR" : "dollar";
    case KEY_M:  //33
        if (lastPressedKey_[KEY_SYM]) return ".";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "M" : "m";
    case KEY_K:  //34
        if (lastPressedKey_[KEY_SYM]) return "\'";
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "K" : "k";
    }
    return "";
}

}  // namespace lilygo_t_keyboard
}  // namespace esphome
