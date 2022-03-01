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
    ESP_LOGCONFIG(TAG, "  Device: %s, offset: %d", Parser::to_hex_string(device_.data).c_str(), device_.offset);
    if (sub_device_.has_value()) ESP_LOGCONFIG(TAG, "  Sub device: %s, offset: %d", Parser::to_hex_string(sub_device_.value().data).c_str(), sub_device_.value().offset);
    if (state_on_.has_value()) ESP_LOGCONFIG(TAG, "  State ON: %s, offset: %d, and_operator: %s, inverted: %s", Parser::to_hex_string(state_on_.value().data).c_str(), state_on_.value().offset, YESNO(state_on_.value().and_operator), YESNO(state_on_.value().inverted));
    if (state_off_.has_value()) ESP_LOGCONFIG(TAG, "  State OFF: %s, offset: %d, and_operator: %s, inverted: %s", Parser::to_hex_string(state_off_.value().data).c_str(), state_off_.value().offset, YESNO(state_off_.value().and_operator), YESNO(state_off_.value().inverted));
    if (command_on_.has_value()) ESP_LOGCONFIG(TAG, "  Command ON: %s", Parser::to_hex_string(command_on_.value().data).c_str());
    if (command_on_.has_value() && command_on_.value().ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command ON Ack: %s", Parser::to_hex_string(command_on_.value().ack).c_str());
    if (command_off_.has_value()) ESP_LOGCONFIG(TAG, "  Command OFF: %s", Parser::to_hex_string(command_off_.value().data).c_str());
    if (command_off_.has_value() && command_off_.value().ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command OFF Ack: %s", Parser::to_hex_string(command_off_.value().ack).c_str());
    if (command_state_.has_value()) ESP_LOGCONFIG(TAG, "  Command State: %s", Parser::to_hex_string(command_state_.value().data).c_str());
    if (command_state_.has_value() && command_state_.value().ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command State Ack: %s", Parser::to_hex_string(command_state_.value().ack).c_str());
    if (state_response_.has_value()) ESP_LOGCONFIG(TAG, "  Data response: %s, offset: %d", Parser::to_hex_string(state_response_.value().data).c_str(), state_response_.value().offset);
    LOG_UPDATE_INTERVAL(this);
}

void WallPadDevice::set_device(hex_t device)
{ 
    device_ = device;
}

void WallPadDevice::set_sub_device(hex_t sub_device)
{
    sub_device_ = sub_device;
}

void WallPadDevice::set_state_on(hex_t state_on)
{ 
    state_on_ = state_on;
}

void WallPadDevice::set_state_off(hex_t state_off)
{
    state_off_ = state_off;
}

void WallPadDevice::set_command_on(cmd_hex_t command_on)
{
    command_on_ = command_on;
}

void WallPadDevice::set_command_on(std::function<cmd_hex_t()> command_on_func)
{
    command_on_func_ = command_on_func;
}

const cmd_hex_t *WallPadDevice::get_command_on()
{
    if (command_on_func_.has_value())
        command_on_ = (*command_on_func_)();
    return &command_on_.value();
}

void WallPadDevice::set_command_off(cmd_hex_t command_off)
{
    command_off_ = command_off;
}

void WallPadDevice::set_command_off(std::function<cmd_hex_t()> command_off_func)
{
    command_off_func_ = command_off_func;
}

const cmd_hex_t *WallPadDevice::get_command_off()
{
    if (command_off_func_.has_value())
        command_off_ = (*command_off_func_)();
    return &command_off_.value();
}

void WallPadDevice::set_command_state(cmd_hex_t command_state)
{
    command_state_ = command_state;
}
/** Response Packet Pattern */
void WallPadDevice::set_state_response(hex_t state_response)
{
    state_response_ = state_response;
}

bool WallPadDevice::is_have_command()
{
    return tx_cmd_queue_.size() > 0 ? true : false;
}

const cmd_hex_t *WallPadDevice::pop_command()
{
    if (state_response_.has_value() && !rx_response_)
        return nullptr;
    rx_response_ = false;
    if (tx_cmd_queue_.size() == 0)
        return nullptr;
    const cmd_hex_t *cmd = tx_cmd_queue_.front();
    tx_cmd_queue_.pop();
    return cmd;
}

void WallPadDevice::ack_ok()
{
    tx_cmd_queue_.size() == 0 ? set_tx_pending(false) : set_tx_pending(true);
}

void WallPadDevice::ack_ng()
{
    ack_ok();
}

void WallPadDevice::set_tx_pending(bool pending)
{
    tx_pending_ = pending;
}

bool WallPadDevice::parse_data(const std::vector<uint8_t>& data)
{
    if (tx_pending_) return false;
    if (state_response_.has_value() && validate(data, &state_response_.value()))    rx_response_ = true;
    else                                                                            rx_response_ = false;
    
    if (!validate(data, &device_)) return false;
    else if (sub_device_.has_value() && !validate(data, &sub_device_.value())) return false;
    if (state_off_.has_value() && validate(data, &state_off_.value()))
    {
        if (!publish(false)) publish(data);
        return true;
    }
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

}  // namespace wallpad
}  // namespace esphome