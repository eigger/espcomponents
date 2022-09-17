#include "uartex_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.fan";

void UARTExFan::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Fan '%s':", device_name_->c_str());
    dump_uartex_device_config(TAG);
    ESP_LOGCONFIG(TAG, "  Support Speed: %s", YESNO(support_speed_));
    if (state_speed_high_.data.size() > 0) ESP_LOGCONFIG(TAG, "  State Speed_high: %s, offset: %d", to_hex_string(state_speed_high_.data).c_str(), state_speed_high_.offset);
    if (state_speed_medium_.data.size() > 0) ESP_LOGCONFIG(TAG, "  State Speed_medium: %s, offset: %d", to_hex_string(state_speed_medium_.data).c_str(), state_speed_medium_.offset);
    if (state_speed_low_.data.size() > 0) ESP_LOGCONFIG(TAG, "  State Speed_low: %s, offset: %d", to_hex_string(state_speed_low_.data).c_str(), state_speed_low_.offset);
    if (command_speed_high_.data.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_high: %s", to_hex_string(command_speed_high_.data).c_str());
    if (command_speed_high_.ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_high Ack: %s", to_hex_string(command_speed_high_.ack).c_str());
    if (command_speed_medium_.data.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_medium: %s", to_hex_string(command_speed_medium_.data).c_str());
    if (command_speed_medium_.ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_medium Ack: %s", to_hex_string(command_speed_medium_.ack).c_str());
    if (command_speed_low_.data.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_low: %s", to_hex_string(command_speed_low_.data).c_str());
    if (command_speed_low_.ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_low Ack: %s", to_hex_string(command_speed_low_.ack).c_str());
}
void UARTExFan::setup()
{
    bool oscillation = false;
    if (command_speed_low_.has_value() || command_speed_medium_.has_value() || command_speed_high_.has_value())
    {
        speed_count_ = 3;
    }

}
void UARTExFan::control(const fan::FanCall &call) override
{
    if (call.get_state().has_value())
      this->state = *call.get_state();
    if (call.get_oscillating().has_value())
      this->oscillating = *call.get_oscillating();
    if (call.get_speed().has_value())
      this->speed = *call.get_speed();
    if (call.get_direction().has_value())
      this->direction = *call.get_direction();
    switch (this->speed)
    {
    case 1:
        if (command_speed_low_.data.size() == 0)
        {
            ESP_LOGW(TAG, "'%s' Not support speed: LOW", device_name_->c_str());
            break;
        }
        push_tx_cmd(&command_speed_low_);
        break;
    case 2:
        if (command_speed_medium_.data.size() == 0)
        {
            ESP_LOGW(TAG, "'%s' Not support speed: MEDIUM", device_name_->c_str());
            break;
        }
        push_tx_cmd(&command_speed_medium_);
        break;
    case 3:
        if (command_speed_high_.data.size() == 0)
        {
            ESP_LOGW(TAG, "'%s' Not support speed: HIGH", device_name_->c_str());
            break;
        }
        push_tx_cmd(&command_speed_high_);
        break;
    default:
        // protect from invalid input
        break;
    }
    this->publish_state();
}

void UARTExFan::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    // Speed high
    if (validate(data, &state_speed_high_))
    {
        speed = 3;
        changed = true;
    }
    // Speed medium
    else if (validate(data, &state_speed_medium_))
    {
        speed = 2;
        changed = true;
    }
    // Speed low
    else if (validate(data, &state_speed_low_))
    {
        pspeed = 1;
        changed = true;
    }
    if (changed) this->publish_state();
    //ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), to_hex_string(data).c_str());
}

}  // namespace uartex
}  // namespace esphome