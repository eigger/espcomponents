#include "wallpad_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wallpad {

static const char *TAG = "wallpad.switch";

void WallPadSwitch::dump_config()
{
    ESP_LOGCONFIG(TAG, "WallPad Switch '%s':", device_name_->c_str());
    dump_wallpad_device_config(TAG);
}

void WallPadSwitch::publish(const std::vector<uint8_t>& data)
{
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(data).c_str());
}

}  // namespace wallpad
}  // namespace esphome
