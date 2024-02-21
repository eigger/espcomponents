#include "uartex_device.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex";

void UARTExDevice::update()
{
    if (!this->command_update_.has_value()) return;
    enqueue_tx_cmd(&this->command_update_.value(), true);
}

void UARTExDevice::dump_uartex_device_config(const char *TAG)
{
    ESP_LOGCONFIG(TAG, "  State: %s, offset: %d", to_hex_string(this->state_.value().data).c_str(), this->state_.value().offset);
    if (this->state_on_.has_value())
        ESP_LOGCONFIG(TAG, "  State ON: %s, offset: %d, inverted: %s", to_hex_string(this->state_on_.value().data).c_str(), this->state_on_.value().offset, YESNO(this->state_on_.value().inverted));
    if (this->state_off_.has_value())
        ESP_LOGCONFIG(TAG, "  State OFF: %s, offset: %d, inverted: %s", to_hex_string(this->state_off_.value().data).c_str(), this->state_off_.value().offset, YESNO(this->state_off_.value().inverted));
    if (this->command_on_.has_value())
        ESP_LOGCONFIG(TAG, "  Command ON: %s", to_hex_string(this->command_on_.value().data).c_str());
    if (this->command_on_.has_value() && this->command_on_.value().ack.size() > 0)
        ESP_LOGCONFIG(TAG, "  Command ON Ack: %s", to_hex_string(this->command_on_.value().ack).c_str());
    if (this->command_off_.has_value())
        ESP_LOGCONFIG(TAG, "  Command OFF: %s", to_hex_string(this->command_off_.value().data).c_str());
    if (this->command_off_.has_value() && this->command_off_.value().ack.size() > 0)
        ESP_LOGCONFIG(TAG, "  Command OFF Ack: %s", to_hex_string(this->command_off_.value().ack).c_str());
    if (this->command_update_.has_value())
        ESP_LOGCONFIG(TAG, "  Command State: %s", to_hex_string(this->command_update_.value().data).c_str());
    if (this->command_update_.has_value() && this->command_update_.value().ack.size() > 0)
        ESP_LOGCONFIG(TAG, "  Command State Ack: %s", to_hex_string(this->command_update_.value().ack).c_str());
    if (this->state_response_.has_value())
        ESP_LOGCONFIG(TAG, "  Data response: %s, offset: %d", to_hex_string(this->state_response_.value().data).c_str(), this->state_response_.value().offset);
    LOG_UPDATE_INTERVAL(this);
}

void UARTExDevice::set_state(state_t state)
{
    this->state_ = state;
}

void UARTExDevice::set_state_on(state_t state_on)
{
    this->state_on_ = state_on;
}

void UARTExDevice::set_state_off(state_t state_off)
{
    this->state_off_ = state_off;
}

void UARTExDevice::set_command_on(cmd_t command)
{
    this->command_on_ = command;
}

void UARTExDevice::set_command_on(std::function<cmd_t(const uint8_t *state, const uint16_t len)> func)
{
    this->command_on_func_ = func;
}

cmd_t *UARTExDevice::get_command_on()
{
    if (this->command_on_func_.has_value())
        this->command_on_ = (*this->command_on_func_)(&last_state_[0], last_state_.size());
    return &this->command_on_.value();
}

void UARTExDevice::set_command_off(cmd_t command)
{
    this->command_off_ = command;
}

void UARTExDevice::set_command_off(std::function<cmd_t(const uint8_t *state, const uint16_t len)> func)
{
    this->command_off_func_ = func;
}

cmd_t *UARTExDevice::get_command_off()
{
    if (this->command_off_func_.has_value())
        this->command_off_ = (*this->command_off_func_)(&last_state_[0], last_state_.size());
    return &this->command_off_.value();
}

void UARTExDevice::set_command_update(cmd_t command)
{
    this->command_update_ = command;
}

void UARTExDevice::set_state_response(state_t state)
{
    this->state_response_ = state;
}

cmd_t *UARTExDevice::dequeue_tx_cmd()
{
    if (this->state_response_.has_value() && !this->rx_response_) return nullptr;
    this->rx_response_ = false;
    if (this->tx_cmd_queue_.size() == 0) return nullptr;
    cmd_t *cmd = this->tx_cmd_queue_.front();
    this->tx_cmd_queue_.pop();
    return cmd;
}

cmd_t *UARTExDevice::dequeue_tx_cmd_low_priority()
{
    if (this->state_response_.has_value() && !this->rx_response_) return nullptr;
    this->rx_response_ = false;
    if (this->tx_cmd_queue_low_priority_.size() == 0) return nullptr;
    cmd_t *cmd = this->tx_cmd_queue_low_priority_.front();
    this->tx_cmd_queue_low_priority_.pop();
    return cmd;
}

bool UARTExDevice::parse_data(const std::vector<uint8_t> &data)
{
    if (this->state_response_.has_value() && verify_state(data, &this->state_response_.value())) this->rx_response_ = true;
    else this->rx_response_ = false;

    if (this->state_.has_value() && !verify_state(data, &this->state_.value())) return false;
    last_state_ = data;
    if (this->state_off_.has_value() && verify_state(data, &this->state_off_.value())) publish(false);
    if (this->state_on_.has_value() && verify_state(data, &this->state_on_.value())) publish(true);
    publish(data);
    return true;
}

void UARTExDevice::publish(const std::vector<uint8_t>& data)
{
}

void UARTExDevice::publish(const bool state)
{
}

void UARTExDevice::enqueue_tx_cmd(cmd_t *cmd, bool low_priority)
{
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
    unsigned int val = 0;
    for (uint16_t i = state.offset, len = 0; i < data.size() && len < state.length; i++, len++)
    {
        val = (val << 8) | data[i];
    }
    return val / powf(10, state.precision);
}

std::string to_hex_string(const std::vector<unsigned char> &data)
{
    char buf[20];
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