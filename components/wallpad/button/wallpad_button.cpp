#include "wallpad_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wallpad {

static const char *TAG = "wallpad.button";

void WallPadButton::dump_config()
{
    ESP_LOGCONFIG(TAG, "WallPad Button '%s':", device_name_->c_str());
    dump_wallpad_device_config(TAG);
}

void WallPadButton::publish(const uint8_t *data, const num_t len)
{
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(&data[0], len).c_str());
}

}  // namespace wallpad
}  // namespace esphome
