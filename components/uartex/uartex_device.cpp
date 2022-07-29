#include "uartex_device.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex";

void UARTExDevice::update()
{
    if (!command_state_.has_value()) return;
    ESP_LOGD(TAG, "'%s' update(): Request current state...", device_name_->c_str());
    push_command(&command_state_.value());
}

void UARTExDevice::dump_uartex_device_config(const char *TAG)
{
    ESP_LOGCONFIG(TAG, "  Device: %s, offset: %d", to_hex_string(device_.data).c_str(), device_.offset);
    if (sub_device_.has_value())
        ESP_LOGCONFIG(TAG, "  Sub device: %s, offset: %d", to_hex_string(sub_device_.value().data).c_str(), sub_device_.value().offset);
    if (state_on_.has_value())
        ESP_LOGCONFIG(TAG, "  State ON: %s, offset: %d, and_operator: %s, inverted: %s", to_hex_string(state_on_.value().data).c_str(), state_on_.value().offset, YESNO(state_on_.value().and_operator), YESNO(state_on_.value().inverted));
    if (state_off_.has_value())
        ESP_LOGCONFIG(TAG, "  State OFF: %s, offset: %d, and_operator: %s, inverted: %s", to_hex_string(state_off_.value().data).c_str(), state_off_.value().offset, YESNO(state_off_.value().and_operator), YESNO(state_off_.value().inverted));
    if (command_on_.has_value())
        ESP_LOGCONFIG(TAG, "  Command ON: %s", to_hex_string(command_on_.value().data).c_str());
    if (command_on_.has_value() && command_on_.value().ack.size() > 0)
        ESP_LOGCONFIG(TAG, "  Command ON Ack: %s", to_hex_string(command_on_.value().ack).c_str());
    if (command_off_.has_value())
        ESP_LOGCONFIG(TAG, "  Command OFF: %s", to_hex_string(command_off_.value().data).c_str());
    if (command_off_.has_value() && command_off_.value().ack.size() > 0)
        ESP_LOGCONFIG(TAG, "  Command OFF Ack: %s", to_hex_string(command_off_.value().ack).c_str());
    if (command_state_.has_value())
        ESP_LOGCONFIG(TAG, "  Command State: %s", to_hex_string(command_state_.value().data).c_str());
    if (command_state_.has_value() && command_state_.value().ack.size() > 0)
        ESP_LOGCONFIG(TAG, "  Command State Ack: %s", to_hex_string(command_state_.value().ack).c_str());
    if (state_response_.has_value())
        ESP_LOGCONFIG(TAG, "  Data response: %s, offset: %d", to_hex_string(state_response_.value().data).c_str(), state_response_.value().offset);
    LOG_UPDATE_INTERVAL(this);
}

void UARTExDevice::set_device(hex_t device)
{
    device_ = device;
}

void UARTExDevice::set_sub_device(hex_t sub_device)
{
    sub_device_ = sub_device;
}

void UARTExDevice::set_state_on(hex_t state_on)
{
    state_on_ = state_on;
}

void UARTExDevice::set_state_off(hex_t state_off)
{
    state_off_ = state_off;
}

void UARTExDevice::set_command_on(cmd_hex_t command_on)
{
    command_on_ = command_on;
}

void UARTExDevice::set_command_on(std::function<cmd_hex_t()> command_on_func)
{
    command_on_func_ = command_on_func;
}

const cmd_hex_t *UARTExDevice::get_command_on()
{
    if (command_on_func_.has_value())
        command_on_ = (*command_on_func_)();
    return &command_on_.value();
}

void UARTExDevice::set_command_off(cmd_hex_t command_off)
{
    command_off_ = command_off;
}

void UARTExDevice::set_command_off(std::function<cmd_hex_t()> command_off_func)
{
    command_off_func_ = command_off_func;
}

const cmd_hex_t *UARTExDevice::get_command_off()
{
    if (command_off_func_.has_value())
        command_off_ = (*command_off_func_)();
    return &command_off_.value();
}

void UARTExDevice::set_command_state(cmd_hex_t command_state)
{
    command_state_ = command_state;
}

void UARTExDevice::set_state_response(hex_t state_response)
{
    state_response_ = state_response;
}

bool UARTExDevice::is_have_command()
{
    return tx_cmd_queue_.size() > 0 ? true : false;
}

const cmd_hex_t *UARTExDevice::pop_command()
{
    if (state_response_.has_value() && !rx_response_) return nullptr;
    rx_response_ = false;
    if (tx_cmd_queue_.size() == 0) return nullptr;
    const cmd_hex_t *cmd = tx_cmd_queue_.front();
    tx_cmd_queue_.pop();
    return cmd;
}

void UARTExDevice::ack_ok()
{   
}

void UARTExDevice::ack_ng()
{
    ack_ok();
}

bool UARTExDevice::parse_data(const std::vector<uint8_t> &data)
{
    if (state_response_.has_value() && validate(data, &state_response_.value()))
        rx_response_ = true;
    else
        rx_response_ = false;

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
    publish(data);
    return true;
}

void UARTExDevice::push_command(const cmd_hex_t *cmd)
{
    tx_cmd_queue_.push(cmd);
}

bool UARTExDevice::equal(const std::vector<uint8_t> &data1, const std::vector<uint8_t> &data2, const num_t offset)
{
    if (data1.size() - offset < data2.size()) return false;
    return std::equal(data1.begin() + offset, data1.begin() + offset + data2.size(), data2.begin());
}

bool UARTExDevice::validate(const std::vector<uint8_t> &data, const hex_t *cmd)
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
    return false;
}

float UARTExDevice::state_to_float(const std::vector<uint8_t>& data, const state_num_t state)
{
    unsigned int val = 0;
    for (num_t i = state.offset, len = 0; i < data.size() && len < state.length; i++, len++)
    {
        val = (val << 8) | data[i];
    }
    return val / powf(10, state.precision);
}

std::string to_hex_string(const std::vector<unsigned char> &data)
{
    char buf[20];
    std::string res;
    for (num_t i = 0; i < data.size(); i++)
    {
        sprintf(buf, "0x%02X ", data[i]);
        res += buf;
    }
    sprintf(buf, "(%d byte)", data.size());
    res += buf;
    return res;
}

}  // namespace uartex
}  // namespace esphome