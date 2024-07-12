#include "uartex_device.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex";

void UARTExDevice::update()
{
    if (get_command_update() == nullptr) return;
    enqueue_tx_cmd(get_command_update(), true);
}

void UARTExDevice::dump_uartex_device_config(const char *TAG)
{
    // ESP_LOGCONFIG(TAG, "  State: %s, offset: %d", to_hex_string(this->state_.value().data).c_str(), this->state_.value().offset);
    // if (this->state_on_.has_value())
    //     ESP_LOGCONFIG(TAG, "  State ON: %s, offset: %d, inverted: %s", to_hex_string(this->state_on_.value().data).c_str(), this->state_on_.value().offset, YESNO(this->state_on_.value().inverted));
    // if (this->state_off_.has_value())
    //     ESP_LOGCONFIG(TAG, "  State OFF: %s, offset: %d, inverted: %s", to_hex_string(this->state_off_.value().data).c_str(), this->state_off_.value().offset, YESNO(this->state_off_.value().inverted));
    // if (this->command_on_.has_value())
    //     ESP_LOGCONFIG(TAG, "  Command ON: %s", to_hex_string(this->command_on_.value().data).c_str());
    // if (this->command_on_.has_value() && this->command_on_.value().ack.size() > 0)
    //     ESP_LOGCONFIG(TAG, "  Command ON Ack: %s", to_hex_string(this->command_on_.value().ack).c_str());
    // if (this->command_off_.has_value())
    //     ESP_LOGCONFIG(TAG, "  Command OFF: %s", to_hex_string(this->command_off_.value().data).c_str());
    // if (this->command_off_.has_value() && this->command_off_.value().ack.size() > 0)
    //     ESP_LOGCONFIG(TAG, "  Command OFF Ack: %s", to_hex_string(this->command_off_.value().ack).c_str());
    // if (this->command_update_.has_value())
    //     ESP_LOGCONFIG(TAG, "  Command State: %s", to_hex_string(this->command_update_.value().data).c_str());
    // if (this->command_update_.has_value() && this->command_update_.value().ack.size() > 0)
    //     ESP_LOGCONFIG(TAG, "  Command State Ack: %s", to_hex_string(this->command_update_.value().ack).c_str());
    // if (this->state_response_.has_value())
    //     ESP_LOGCONFIG(TAG, "  Data response: %s, offset: %d", to_hex_string(this->state_response_.value().data).c_str(), this->state_response_.value().offset);
    LOG_UPDATE_INTERVAL(this);
}

state_t* UARTExDevice::get_state()
{
    return get_state("state");
}

state_t* UARTExDevice::get_state_on()
{
    return get_state("state_on");
}

state_t* UARTExDevice::get_state_off()
{
    return get_state("state_off");
}

state_t* UARTExDevice::get_state_response()
{
    return get_state("state_response");
}

optional<float> UARTExDevice::get_state_func(std::string name, const uint8_t *data, const uint16_t len)
{
    return (this->state_func_map_[name])(data, len);
}

cmd_t *UARTExDevice::get_command_on()
{
    return get_command("command_on");
}

cmd_t *UARTExDevice::get_command_off()
{
    return get_command("command_off");
}

cmd_t* UARTExDevice::get_command_update()
{
    return get_command("command_update");
}

bool UARTExDevice::has_state_func(std::string name)
{
    if (this->state_func_map_.find(name) != this->state_func_map_.end()) return true;
    return false;
}

cmd_t* UARTExDevice::get_command(std::string name, const float x)
{
    if (this->command_param_func_map_.find(name) != this->command_param_func_map_.end())
    {
        this->command_map_[name] = (this->command_param_func_map_[name])(x);
        return &this->command_map_[name];
    }
    else if (this->command_func_map_.find(name) != this->command_func_map_.end())
    {
        this->command_map_[name] = (this->command_func_map_[name])();
        return &this->command_map_[name];
    }
    else if (this->command_map_.find(name) != this->command_map_.end())
    {
        return &this->command_map_[name];
    }
    return nullptr;
}

state_t* UARTExDevice::get_state(std::string name)
{
    if (this->state_map_.find(name) != this->state_map_.end())
    {
        return &this->state_map_[name];
    }
    return nullptr;
}

state_num_t* UARTExDevice::get_state_num(std::string name)
{
    if (this->state_num_map_.find(name) != this->state_num_map_.end())
    {
        return &this->state_num_map_[name];
    }
    return nullptr;
}

const cmd_t *UARTExDevice::dequeue_tx_cmd()
{
    if (get_state_response() && !this->rx_response_) return nullptr;
    this->rx_response_ = false;
    if (this->tx_cmd_queue_.size() == 0) return nullptr;
    const cmd_t *cmd = this->tx_cmd_queue_.front();
    this->tx_cmd_queue_.pop();
    return cmd;
}

const cmd_t *UARTExDevice::dequeue_tx_cmd_low_priority()
{
    if (get_state_response() && !this->rx_response_) return nullptr;
    this->rx_response_ = false;
    if (this->tx_cmd_queue_low_priority_.size() == 0) return nullptr;
    const cmd_t *cmd = this->tx_cmd_queue_low_priority_.front();
    this->tx_cmd_queue_low_priority_.pop();
    return cmd;
}

bool UARTExDevice::parse_data(const std::vector<uint8_t> &data)
{
    if (get_state_response() && verify_state(data, get_state_response())) this->rx_response_ = true;
    else this->rx_response_ = false;
    if (get_state() && !verify_state(data, get_state())) return false;
    if (get_state_off() && verify_state(data, get_state_off())) publish(false);
    if (get_state_on() && verify_state(data, get_state_on())) publish(true);
    publish(data);
    return true;
}

void UARTExDevice::publish(const std::vector<uint8_t>& data)
{
}

void UARTExDevice::publish(const bool state)
{
}

void UARTExDevice::enqueue_tx_cmd(const cmd_t *cmd, bool low_priority)
{
    if (cmd == nullptr) return;
    if (cmd->data.size() == 0) return;
    if (low_priority) this->tx_cmd_queue_low_priority_.push(cmd);
    else this->tx_cmd_queue_.push(cmd);
}

bool equal(const std::vector<uint8_t> &data1, const std::vector<uint8_t> &data2, const uint16_t offset)
{
    if (data1.size() - offset < data2.size()) return false;
    return std::equal(data1.begin() + offset, data1.begin() + offset + data2.size(), data2.begin());
}

const std::vector<uint8_t> masked_data(const std::vector<uint8_t> &data, const state_t *state)
{
    std::vector<uint8_t> masked_data = data;
    for(size_t i = state->offset, j = 0; i < data.size() && j < state->mask.size(); i++, j++)
    {
        masked_data[i] &= state->mask[j];
    }
    return masked_data;
}

bool verify_state(const std::vector<uint8_t> &data, const state_t *state)
{
    if (state->mask.size() == 0)    return equal(data, state->data, state->offset) ? !state->inverted : state->inverted;
    else                            return equal(masked_data(data, state), state->data, state->offset) ? !state->inverted : state->inverted;
    return false;
}

float state_to_float(const std::vector<uint8_t>& data, const state_num_t state)
{
    int32_t val = 0;
    for (uint16_t i = state.offset, len = 0; i < data.size() && len < state.length; i++, len++)
    {
        val = (val << 8) | (int8_t)data[i];
    }
    return val / powf(10, state.precision);
}

std::string to_hex_string(const std::vector<unsigned char> &data)
{
    char buf[10];
    std::string res;
    for (uint16_t i = 0; i < data.size(); i++)
    {
        sprintf(buf, "0x%02X ", data[i]);
        res += buf;
    }
    sprintf(buf, "(%d byte)", data.size());
    res += buf;
    return res;
}

std::string to_hex_string(const uint8_t *data, const uint16_t len)
{
    char buf[5];
    std::string res;
    for (uint16_t i = 0; i < len; i++)
    {
        sprintf(buf, "%02X", data[i]);
        res += buf;
    }
    sprintf(buf, "(%d)", len);
    res += buf;
    return res;
}

unsigned long elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}

unsigned long get_time()
{
    return millis();
}

}  // namespace uartex
}  // namespace esphome