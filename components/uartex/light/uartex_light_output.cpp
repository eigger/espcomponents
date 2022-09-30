#include "uartex_light_output.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"


namespace esphome {
namespace uartex {

static const char *TAG = "uartex.light";

void UARTExLightOutput::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx LightOutput(Binary) '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
}

void UARTExLightOutput::publish(const std::vector<uint8_t>& data)
{
}

void UARTExLightOutput::publish_state(bool state)
{
    if (light_ == nullptr || state == this->state_)return;
    this->state_ = state;
    this->light_->remote_values.set_state(state);
    if (api::global_api_server->is_connected()) api::global_api_server->on_light_update(this->light_);
}

}  // namespace uartex
}  // namespace esphome
