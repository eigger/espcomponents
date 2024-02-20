#include "uartex_light_output.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"


namespace esphome {
namespace uartex {

static const char *TAG = "uartex.light";

void UARTExLightOutput::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx LightOutput(Binary) '%s':", light_state_->get_name().c_str());
    dump_uartex_device_config(TAG);
}

void UARTExLightOutput::publish_state(bool state)
{
    if (light_state_ == nullptr || state == this->state_)return;
    this->state_ = state;
    auto call = light_state_->make_call();
    call.set_state(state);
    call.perform();
}

}  // namespace uartex
}  // namespace esphome
