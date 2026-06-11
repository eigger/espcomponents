#include "tca8418_keyboard.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tca8418_keyboard {

static const char *const TAG = "tca8418_keyboard";

// TCA8418 registers
static const uint8_t REG_CFG             = 0x01;
static const uint8_t REG_INT_STAT        = 0x02;
static const uint8_t REG_KEY_EVENT_A     = 0x04;
static const uint8_t REG_GPIO_INT_STAT_1 = 0x11;
static const uint8_t REG_GPIO_INT_EN_1   = 0x1A;
static const uint8_t REG_KP_GPIO_1       = 0x1D;
static const uint8_t REG_GPI_EM_1        = 0x20;
static const uint8_t REG_GPIO_DIR_1      = 0x23;
static const uint8_t REG_GPIO_INT_LVL_1  = 0x26;

// CFG register bits
static const uint8_t CFG_KE_IEN  = 0x01;
static const uint8_t CFG_GPI_IEN = 0x02;

// INT_STAT register bits
static const uint8_t INT_STAT_K_INT   = 0x01;
static const uint8_t INT_STAT_GPI_INT = 0x02;

// TCA8418 key-event FIFO is 10 entries deep
static const uint8_t FIFO_DEPTH = 10;

// Modifier key positions on the Cardputer layout
static const int FN_ROW = 2,    FN_COL = 0;
static const int SHIFT_ROW = 2, SHIFT_COL = 1;
static const int CTRL_ROW = 3,  CTRL_COL = 0;
static const int OPT_ROW = 3,   OPT_COL = 1;
static const int ALT_ROW = 3,   ALT_COL = 2;

// M5Cardputer ADV 4x14 layout
static const char *const KEYMAP[TCA8418Keyboard::ROWS][TCA8418Keyboard::COLS] = {
    {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Backspace"},
    {"Tab", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\"},
    {"Fn", "Shift", "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "Enter"},
    {"Ctrl", "Opt", "Alt", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "Space"},
};

static const char *const KEYMAP_SHIFT[TCA8418Keyboard::ROWS][TCA8418Keyboard::COLS] = {
    {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "Backspace"},
    {"Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "|"},
    {"Fn", "Shift", "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"", "Enter"},
    {"Ctrl", "Opt", "Alt", "Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "Space"},
};

static const char *const KEYMAP_FN[TCA8418Keyboard::ROWS][TCA8418Keyboard::COLS] = {
    {"Esc", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "Del"},
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "Up", nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "Left", "Down", "Right", nullptr},
};

void TCA8418Keyboard::setup()
{
    ESP_LOGCONFIG(TAG, "Setting up TCA8418Keyboard...");
    if (this->irq_pin_ != nullptr)
    {
        this->irq_pin_->setup();
    }

    // all pins to GPIO input, key-event mode, falling-edge interrupts
    for (uint8_t i = 0; i < 3; i++)
    {
        if (!write_reg_(REG_GPIO_DIR_1 + i, 0x00))
        {
            ESP_LOGE(TAG, "TCA8418 not responding");
            this->mark_failed();
            return;
        }
        write_reg_(REG_GPI_EM_1 + i, 0xFF);
        write_reg_(REG_GPIO_INT_LVL_1 + i, 0x00);
        write_reg_(REG_GPIO_INT_EN_1 + i, 0xFF);
    }

    // keypad matrix: 7 rows x 8 columns (M5Cardputer ADV)
    write_reg_(REG_KP_GPIO_1, 0x7F);
    write_reg_(REG_KP_GPIO_1 + 1, 0xFF);
    write_reg_(REG_KP_GPIO_1 + 2, 0x00);

    flush_events_();

    // enable key-event + GPI interrupts (hardware debounce stays on by default)
    uint8_t cfg = 0;
    if (read_reg_(REG_CFG, &cfg))
    {
        write_reg_(REG_CFG, cfg | CFG_KE_IEN | CFG_GPI_IEN);
    }

    if (press_key_ != nullptr) press_key_->publish_state("None");
    if (release_key_ != nullptr) release_key_->publish_state("None");
}

void TCA8418Keyboard::dump_config()
{
    ESP_LOGCONFIG(TAG, "TCA8418Keyboard");
    ESP_LOGCONFIG(TAG, "  Model: cardputer_adv");
    LOG_I2C_DEVICE(this);
    if (this->irq_pin_ != nullptr)
    {
        LOG_PIN("  IRQ Pin: ", this->irq_pin_);
    }
}

void TCA8418Keyboard::loop()
{
    // INT is active-low; while it is high there is nothing queued, so skip I2C
    if (this->irq_pin_ != nullptr && this->irq_pin_->digital_read()) return;

    // Drain the hardware key-event FIFO. Each entry is one press/release; an
    // entry of 0 means the FIFO is empty. Bound the loop to the FIFO depth so a
    // misbehaving chip can never hang the loop.
    for (uint8_t i = 0; i < FIFO_DEPTH; i++)
    {
        uint8_t event = 0;
        if (!read_reg_(REG_KEY_EVENT_A, &event) || event == 0) break;
        process_event_(event);
    }

    // Clear the interrupt flags so the chip releases the INT line
    write_reg_(REG_INT_STAT, INT_STAT_K_INT | INT_STAT_GPI_INT);
}

void TCA8418Keyboard::process_event_(uint8_t event)
{
    bool pressed = (event & 0x80) != 0;
    uint8_t key = (event & 0x7F) - 1;
    uint8_t raw_row = key / 10;
    uint8_t raw_col = key % 10;

    int row = 0, col = 0;
    if (!remap_(raw_row, raw_col, &row, &col)) return;

    if (pressed_[row][col] == pressed) return;
    pressed_[row][col] = pressed;
    if (pressed) on_press_key_(row, col);
    else         on_release_key_(row, col);
}

// Convert the TCA8418 7x8 scan coordinate to the model's logical layout.
bool TCA8418Keyboard::remap_(uint8_t raw_row, uint8_t raw_col, int *row, int *col)
{
    switch (model_)
    {
    case MODEL_CARDPUTER_ADV:
    default:
        *col = raw_row * 2 + (raw_col > 3 ? 1 : 0);
        *row = (raw_col + 4) % 4;
        break;
    }
    return (*row >= 0 && *row < ROWS && *col >= 0 && *col < COLS);
}

void TCA8418Keyboard::on_press_key_(int row, int col)
{
    ESP_LOGV(TAG, "press key (%d, %d)", row, col);
    if (is_modifier_(row, col)) return;
    std::string key = key_to_string_(row, col);
    if (press_key_ != nullptr) press_key_->publish_state(key);
    this->press_callback_.call(key);
}

void TCA8418Keyboard::on_release_key_(int row, int col)
{
    ESP_LOGV(TAG, "release key (%d, %d)", row, col);
    if (is_modifier_(row, col)) return;
    std::string key = key_to_string_(row, col);
    if (release_key_ != nullptr) release_key_->publish_state(key);
    this->release_callback_.call(key);
}

bool TCA8418Keyboard::is_modifier_(int row, int col)
{
    return (row == FN_ROW && col == FN_COL) ||
           (row == SHIFT_ROW && col == SHIFT_COL) ||
           (row == CTRL_ROW && col == CTRL_COL) ||
           (row == OPT_ROW && col == OPT_COL) ||
           (row == ALT_ROW && col == ALT_COL);
}

std::string TCA8418Keyboard::key_to_string_(int row, int col)
{
    const char *name = nullptr;
    if (pressed_[FN_ROW][FN_COL])
    {
        name = KEYMAP_FN[row][col];
    }
    if (name == nullptr)
    {
        name = pressed_[SHIFT_ROW][SHIFT_COL] ? KEYMAP_SHIFT[row][col] : KEYMAP[row][col];
    }
    std::string str = name;
    if (pressed_[ALT_ROW][ALT_COL])   str = "Alt+" + str;
    if (pressed_[OPT_ROW][OPT_COL])   str = "Opt+" + str;
    if (pressed_[CTRL_ROW][CTRL_COL]) str = "Ctrl+" + str;
    return str;
}

void TCA8418Keyboard::flush_events_()
{
    uint8_t event = 0;
    for (uint8_t i = 0; i < FIFO_DEPTH; i++)
    {
        if (!read_reg_(REG_KEY_EVENT_A, &event) || event == 0) break;
    }
    uint8_t value = 0;
    for (uint8_t i = 0; i < 3; i++)
    {
        read_reg_(REG_GPIO_INT_STAT_1 + i, &value);
    }
    write_reg_(REG_INT_STAT, INT_STAT_K_INT | INT_STAT_GPI_INT);
}

bool TCA8418Keyboard::read_reg_(uint8_t reg, uint8_t *value)
{
    if (this->is_failed()) return false;
    return this->read_byte(reg, value);
}

bool TCA8418Keyboard::write_reg_(uint8_t reg, uint8_t value)
{
    if (this->is_failed()) return false;
    return this->write_byte(reg, value);
}

}  // namespace tca8418_keyboard
}  // namespace esphome
