#include "uartex_light_output.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"


namespace esphome {
namespace uartex {

static const char *TAG = "uartex.light";

void UartExLightOutput::dump_config()
{
    ESP_LOGCONFIG(TAG, "UartEx LightOutput(Binary) '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

void UartExLightOutput::publish(const uint8_t *data, const num_t len)
{
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(&data[0], len).c_str());
}

void UartExLightOutput::publish_state(bool state)
{
    if (light_ == nullptr || state == this->state_)return;
    ESP_LOGD(TAG, "'%s' UartExLightOutput::publish_state(%s)", device_name_->c_str(), state ? "True" : "False");
    this->state_ = state;
    this->light_->remote_values.set_state(state);
    if (api::global_api_server->is_connected()) api::global_api_server->on_light_update(this->light_);
}

}  // namespace uartex
}  // namespace esphome
