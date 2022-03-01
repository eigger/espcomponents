#include "wallpad_light_output.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"


namespace esphome {
namespace wallpad {

static const char *TAG = "wallpad.light";

void WallPadLightOutput::dump_config()
{
    ESP_LOGCONFIG(TAG, "WallPad LightOutput(Binary) '%s':", device_name_->c_str());
    dump_wallpad_device_config(TAG);
}

void WallPadLightOutput::publish(const std::vector<uint8_t>& data)
{
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(data).c_str());
}

void WallPadLightOutput::publish_state(bool state)
{
    if (light_ == nullptr || state == this->state_)return;
    ESP_LOGD(TAG, "'%s' WallPadLightOutput::publish_state(%s)", device_name_->c_str(), state ? "True" : "False");
    this->state_ = state;
    this->light_->remote_values.set_state(state);
    if (api::global_api_server->is_connected()) api::global_api_server->on_light_update(this->light_);
}

}  // namespace wallpad
}  // namespace esphome
