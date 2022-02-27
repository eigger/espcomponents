#include "wallpad_fan.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace wallpad {

static const char *TAG = "wallpad.fan";

void WallPadFan::dump_config()
{
    ESP_LOGCONFIG(TAG, "WallPad Fan '%s':", device_name_->c_str());
    dump_wallpad_device_config(TAG);
    ESP_LOGCONFIG(TAG, "  Support Speed: %s", YESNO(support_speed_));
    if (state_speed_high_.data.size() > 0) ESP_LOGCONFIG(TAG, "  State Speed_high: %s, offset: %d", hexencode(&state_speed_high_.data[0], state_speed_high_.data.size()).c_str(), state_speed_high_.offset);
    if (state_speed_medium_.data.size() > 0) ESP_LOGCONFIG(TAG, "  State Speed_medium: %s, offset: %d", hexencode(&state_speed_medium_.data[0], state_speed_medium_.data.size()).c_str(), state_speed_medium_.offset);
    if (state_speed_low_.data.size() > 0) ESP_LOGCONFIG(TAG, "  State Speed_low: %s, offset: %d", hexencode(&state_speed_low_.data[0], state_speed_low_.data.size()).c_str(), state_speed_low_.offset);
    if (command_speed_high_.data.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_high: %s", hexencode(&command_speed_high_.data[0], command_speed_high_.data.size()).c_str());
    if (command_speed_high_.ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_high Ack: %s", hexencode(&command_speed_high_.ack[0], command_speed_high_.ack.size()).c_str());
    if (command_speed_medium_.data.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_medium: %s", hexencode(&command_speed_medium_.data[0], command_speed_medium_.data.size()).c_str());
    if (command_speed_medium_.ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_medium Ack: %s", hexencode(&command_speed_medium_.ack[0], command_speed_medium_.ack.size()).c_str());
    if (command_speed_low_.data.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_low: %s", hexencode(&command_speed_low_.data[0], command_speed_low_.data.size()).c_str());
    if (command_speed_low_.ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command Speed_low Ack: %s", hexencode(&command_speed_low_.ack[0], command_speed_low_.ack.size()).c_str());
}
void WallPadFan::setup()
{
    bool oscillation = false;
    this->support_speed_ = command_speed_low_.data.size() > 0 || command_speed_medium_.data.size() > 0 || command_speed_high_.data.size() > 0;
    auto traits = fan::FanTraits(oscillation, this->support_speed_, false, this->support_speed_ ? 3 : 1);
    this->fan_->set_traits(traits);
    this->fan_->add_on_state_callback([this](){ this->perform(); });
}

void WallPadFan::perform()
{
    // ON_OFF
    if (this->fan_->state != this->state_)
    {
        this->state_ = this->fan_->state;
        ESP_LOGD(TAG, "'%s' Turning %s.", device_name_->c_str(), this->state_ ? "ON" : "OFF");
        push_command(this->state_ ? this->get_command_on() : this->get_command_off());
    }
    // Speed
    else if (this->support_speed_ && this->state_ && this->speed_ != this->fan_->speed)
    {
        this->speed_ = this->fan_->speed;
        switch (this->speed_)
        {
        case 1:
            if (command_speed_low_.data.size() == 0)
            {
                ESP_LOGW(TAG, "'%s' Not support speed: LOW", device_name_->c_str());
                break;
            }
            push_command(&command_speed_low_);
            break;
        case 2:
            if (command_speed_medium_.data.size() == 0)
            {
                ESP_LOGW(TAG, "'%s' Not support speed: MEDIUM", device_name_->c_str());
                break;
            }
            push_command(&command_speed_medium_);
            break;
        case 3:
            if (command_speed_high_.data.size() == 0)
            {
                ESP_LOGW(TAG, "'%s' Not support speed: HIGH", device_name_->c_str());
                break;
            }
            push_command(&command_speed_high_);
            break;
        default:
            // protect from invalid input
            break;
        }
    }
}

void WallPadFan::publish(const uint8_t *data, const num_t len)
{
    // Speed high
    if (compare(&data[0], len, &state_speed_high_))
    {
        publish_state(3);
        return;
    }
    // Speed medium
    else if (compare(&data[0], len, &state_speed_medium_))
    {
        publish_state(2);
        return;
    }
    // Speed low
    else if (compare(&data[0], len, &state_speed_low_))
    {
        publish_state(1);
        return;
    }
    ESP_LOGW(TAG, "'%s' State not found: %s", device_name_->c_str(), hexencode(&data[0], len).c_str());
}

void WallPadFan::publish_state(bool state)
{
    if (state == this->fan_->state) return;
    ESP_LOGD(TAG, "'%s' WallPadFan::publish_state(%s)", device_name_->c_str(), state ? "True" : "False");
    this->state_ = state;
    this->fan_->state = state;
    if (api::global_api_server->is_connected()) api::global_api_server->on_fan_update(this->fan_);
}

void WallPadFan::publish_state(int speed)
{
    if (!this->state_ || speed == this->speed_) return;
    else this->speed_ = speed;
    std::string str_speed = "";
    switch (this->speed_)
    {
    case 1:
        str_speed = "LOW";
        break;
    case 2:
        str_speed = "MEDIUM";
        break;
    case 3:
        str_speed = "HIGH";
        break;
    default:
        str_speed = "Unknow";
        break;
    }
    ESP_LOGD(TAG, "'%s' WallPadFan::publish_state(%s)", device_name_->c_str(), str_speed.c_str());
    this->fan_->speed = speed;
    if (api::global_api_server->is_connected()) api::global_api_server->on_fan_update(this->fan_);
}

}  // namespace wallpad
}  // namespace esphome