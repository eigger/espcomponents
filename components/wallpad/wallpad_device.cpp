#include "wallpad_device.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace wallpad {
static const char *TAG = "wallpad";
void WallPadDevice::update()
{
    if (!command_state_.has_value()) return;
    ESP_LOGD(TAG, "'%s' update(): Request current state...", device_name_->c_str());
    push_command(&command_state_.value());
}

void WallPadDevice::dump_wallpad_device_config(const char *TAG)
{
    ESP_LOGCONFIG(TAG, "  Device: %s, offset: %d", hexencode(&device_.data[0], device_.data.size()).c_str(), device_.offset);
    if (sub_device_.has_value()) ESP_LOGCONFIG(TAG, "  Sub device: %s, offset: %d", hexencode(&sub_device_.value().data[0], sub_device_.value().data.size()).c_str(), sub_device_.value().offset);
    if (state_on_.has_value()) ESP_LOGCONFIG(TAG, "  State ON: %s, offset: %d, and_operator: %s, inverted: %s", hexencode(&state_on_.value().data[0], state_on_.value().data.size()).c_str(), state_on_.value().offset, YESNO(state_on_.value().and_operator), YESNO(state_on_.value().inverted));
    if (state_off_.has_value()) ESP_LOGCONFIG(TAG, "  State OFF: %s, offset: %d, and_operator: %s, inverted: %s", hexencode(&state_off_.value().data[0], state_off_.value().data.size()).c_str(), state_off_.value().offset, YESNO(state_off_.value().and_operator), YESNO(state_off_.value().inverted));
    if (command_on_.has_value()) ESP_LOGCONFIG(TAG, "  Command ON: %s", hexencode(&command_on_.value().data[0], command_on_.value().data.size()).c_str());
    if (command_on_.has_value() && command_on_.value().ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command ON Ack: %s", hexencode(&command_on_.value().ack[0], command_on_.value().ack.size()).c_str());
    if (command_off_.has_value()) ESP_LOGCONFIG(TAG, "  Command OFF: %s", hexencode(&command_off_.value().data[0], command_off_.value().data.size()).c_str());
    if (command_off_.has_value() && command_off_.value().ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command OFF Ack: %s", hexencode(&command_off_.value().ack[0], command_off_.value().ack.size()).c_str());
    if (command_state_.has_value()) ESP_LOGCONFIG(TAG, "  Command State: %s", hexencode(&command_state_.value().data[0], command_state_.value().data.size()).c_str());
    if (command_state_.has_value() && command_state_.value().ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command State Ack: %s", hexencode(&command_state_.value().ack[0], command_state_.value().ack.size()).c_str());
    LOG_UPDATE_INTERVAL(this);
}

bool WallPadDevice::parse_data(const std::vector<uint8_t>& data)
{
    if (tx_pending_) return false;

    if (!validate(data, &device_)) return false;
    else if (sub_device_.has_value() && !validate(data, &sub_device_.value())) return false;

    // Turn OFF Message
    if (state_off_.has_value() && validate(data, &state_off_.value()))
    {
        if (!publish(false)) publish(data);
        return true;
    }
    // Turn ON Message
    else if (state_on_.has_value() && validate(data, &state_on_.value()))
    {
        if (!publish(true)) publish(data);
        return true;
    }

    // Other Message
    publish(data);
    return true;
}

void WallPadDevice::push_command(const cmd_hex_t *cmd)
{
    set_tx_pending(true);
    tx_cmd_queue_.push(cmd);
}

bool WallPadDevice::equal(const std::vector<uint8_t>& data1, const std::vector<uint8_t>& data2,  const num_t offset)
{
    if (data1.size() - offset < data2.size()) return false;
    return std::equal(data1.begin() + offset, data1.begin() + offset + data2.size(), data2.begin());
}

bool WallPadDevice::validate(const std::vector<uint8_t>& data, const hex_t *cmd)
{
    if (!cmd->and_operator) return equal(data, cmd->data, cmd->offset) ? !cmd->inverted : cmd->inverted;
    else if (data.size() - cmd->offset > 0 && cmd->data.size() > 0)
    {
        uint8_t val = data[cmd->offset] & (cmd->data[0]);
        if (cmd->data.size() == 1) return val ? !cmd->inverted : cmd->inverted;
        else
        {
            bool ret = false;
            for (num_t i = 1; i < cmd->data.size(); i++)
            {
                if (val == cmd->data[i])
                {
                    ret = true;
                    break;
                }
            }
            return ret ? !cmd->inverted : cmd->inverted;
        }
    }
    else return false;
}

float WallPadDevice::hex_to_float(const uint8_t *data, const num_t len, const num_t precision)
{
    unsigned int val = 0;
    for (num_t i = 0; i < len; i++)
    {
        val = (val << 8) | data[i];
    }
    return val / powf(10, precision);
}

std::string hexencode(const std::vector<uint8_t>& raw_data)
{
    return hexencode(&raw_data[0], raw_data.size());
}
std::string hexencode(const uint8_t *raw_data, num_t len)
{
    char buf[20];
    std::string res;
    for (num_t i = 0; i < len; i++)
    {
        sprintf(buf, "0x%02X ", raw_data[i]);
        res += buf;
    }
    sprintf(buf, "(%d byte)", len);
    res += buf;
    return res;
}



}  // namespace wallpad
}  // namespace esphome