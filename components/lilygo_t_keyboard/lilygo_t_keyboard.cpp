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
}

void LilygoTKeyboard::on_release_key(int key)
{

}


}  // namespace lilygo_t_keyboard
}  // namespace esphome
