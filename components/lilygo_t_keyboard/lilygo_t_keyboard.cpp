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
    if (press_key_ != nullptr) press_key_->publish_state("None");
    if (release_key_ != nullptr) release_key_->publish_state("None");
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
    ESP_LOGV(TAG, "press key (%d)", key);
    if (key == KEY_LSHIFT || key == KEY_RSHIFT || key == KEY_SYM || key == KEY_ALT) return;
    if (press_key_ != nullptr) press_key_->publish_state(key_to_string(key));
}

void LilygoTKeyboard::on_release_key(int key)
{
    ESP_LOGV(TAG, "release key (%d)", key);
    if (key == KEY_LSHIFT || key == KEY_RSHIFT || key == KEY_SYM || key == KEY_ALT) return;
    if (release_key_ != nullptr) release_key_->publish_state(key_to_string(key));
}

std::string LilygoTKeyboard::key_to_string(int key)
{
    std::string str;
    switch(key)
    {
    case KEY_Q:  //0
        if (lastPressedKey_[KEY_SYM]) return "#";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "Q" : "q";
        break;
    case KEY_W:  //1
        if (lastPressedKey_[KEY_SYM]) return "1";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "W" : "w";
        break;
    case KEY_SYM:  //2
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "SYM" : "sym";
        break;
    case KEY_A:  //3
        if (lastPressedKey_[KEY_SYM]) return "*";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "A" : "a";
        break;
    case KEY_ALT:  //4
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "ALT" : "alt";
        break;
    case KEY_SPACE:  //5
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "SPACE" : "space";
        break;
    case KEY_MIC:  //6
        if (lastPressedKey_[KEY_SYM]) return "0";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "MIC" : "mic";
        break;
    case KEY_E:  //7
        if (lastPressedKey_[KEY_SYM]) return "2";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "E" : "e";
        break;
    case KEY_S:  //8
        if (lastPressedKey_[KEY_SYM]) return "4";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "S" : "s";
        break;
    case KEY_D:  //9
        if (lastPressedKey_[KEY_SYM]) return "5";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "D" : "d";
        break;
    case KEY_P:  //10
        if (lastPressedKey_[KEY_SYM]) return "@";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "P" : "p";
        break;
    case KEY_X:  //11
        if (lastPressedKey_[KEY_SYM]) return "8";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "X" : "x";
        break;
    case KEY_Z:  //12
        if (lastPressedKey_[KEY_SYM]) return "7";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "Z" : "z";
        break;
    case KEY_LSHIFT:  //13
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "LSHIFT" : "lshift";
        break;
    case KEY_R:  //14
        if (lastPressedKey_[KEY_SYM]) return "3";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "R" : "r";
        break;
    case KEY_G:  //15
        if (lastPressedKey_[KEY_SYM]) return "/";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "G" : "g";
        break;
    case KEY_T:  //16
        if (lastPressedKey_[KEY_SYM]) return "()";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "T" : "t";
        break;
    case KEY_RSHIFT:  //17
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "RSHIFT" : "rshift";
        break;
    case KEY_V:  //18
        if (lastPressedKey_[KEY_SYM]) return "?";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "V" : "v";
        break;
    case KEY_C:  //19
        if (lastPressedKey_[KEY_SYM]) return "9";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "C" : "c";
        break;
    case KEY_F:  //20
        if (lastPressedKey_[KEY_SYM]) return "6";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "F" : "f";
        break;
    case KEY_U:  //21
        if (lastPressedKey_[KEY_SYM]) return "_";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "U" : "u";
        break;
    case KEY_H:  //22
        if (lastPressedKey_[KEY_SYM]) return ":";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "H" : "h";
        break;
    case KEY_Y:  //23
        if (lastPressedKey_[KEY_SYM]) return ")";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "Y" : "y";
        break;
    case KEY_ENTER:  //24
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "ENTER" : "enter";
        break;
    case KEY_B:  //25
        if (lastPressedKey_[KEY_SYM]) return "!";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "B" : "b";
        break;
    case KEY_N:  //26
        if (lastPressedKey_[KEY_SYM]) return ",";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "N" : "n";
        break;
    case KEY_J:  //27
        if (lastPressedKey_[KEY_SYM]) return ";";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "J" : "j";
        break;
    case KEY_O:  //28
        if (lastPressedKey_[KEY_SYM]) return "+";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "O" : "o";
        break;
    case KEY_L:  //29
        if (lastPressedKey_[KEY_SYM]) return "\"";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "L" : "l";
        break;
    case KEY_I:  //30
        if (lastPressedKey_[KEY_SYM]) return "-";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "I" : "i";
        break;
    case KEY_BS:  //31
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "BS" : "bs";
        break;
    case KEY_DOLLAR:  //32
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "DOLLAR" : "dollar";
        break;
    case KEY_M:  //33
        if (lastPressedKey_[KEY_SYM]) return ".";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "M" : "m";
        break;
    case KEY_K:  //34
        if (lastPressedKey_[KEY_SYM]) return "\'";
        str = lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "K" : "k";
        break;
    }
    if (lastPressedKey_[KEY_ALT])
    {
        return lastPressedKey_[KEY_LSHIFT] || lastPressedKey_[KEY_RSHIFT] ? "ALT+" + str : "alt+" + str;
    }
    return str;
}

}  // namespace lilygo_t_keyboard
}  // namespace esphome
