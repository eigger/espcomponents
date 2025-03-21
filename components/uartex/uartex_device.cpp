#include "uartex_device.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex";

void UARTExDevice::update()
{
    enqueue_tx_cmd(get_command_update(), true);
}

void UARTExDevice::uartex_dump_config(const char* TAG)
{
    state_t* state = get_state();
    if (state) ESP_LOGCONFIG(TAG, "  State: %s, offset: %d, inverted: %s", to_hex_string(state->data).c_str(), state->offset, YESNO(state->inverted));
    state = get_state_on();
    if (state) ESP_LOGCONFIG(TAG, "  State ON: %s, offset: %d, inverted: %s", to_hex_string(state->data).c_str(), state->offset, YESNO(state->inverted));
    state = get_state_off();
    if (state) ESP_LOGCONFIG(TAG, "  State OFF: %s, offset: %d, inverted: %s", to_hex_string(state->data).c_str(), state->offset, YESNO(state->inverted));
    state = get_state_response();
    if (state) ESP_LOGCONFIG(TAG, "  State RESP: %s, offset: %d, inverted: %s", to_hex_string(state->data).c_str(), state->offset, YESNO(state->inverted));

    cmd_t* cmd = get_command_on();
    if (cmd) ESP_LOGCONFIG(TAG, "  Command ON: %s, ACK: %s", to_hex_string(cmd->data).c_str(), to_hex_string(cmd->ack).c_str());
    cmd = get_command_off();
    if (cmd) ESP_LOGCONFIG(TAG, "  Command OFF: %s, ACK: %s", to_hex_string(cmd->data).c_str(), to_hex_string(cmd->ack).c_str());
    cmd = get_command_update();
    if (cmd) ESP_LOGCONFIG(TAG, "  Command UPDATE: %s, ACK: %s", to_hex_string(cmd->data).c_str(), to_hex_string(cmd->ack).c_str());
    LOG_UPDATE_INTERVAL(this);
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

bool UARTExDevice::parse_data(const std::vector<uint8_t>& data)
{
    if (verify_state(data, get_state_response())) this->rx_response_ = true;
    else this->rx_response_ = false;
    if (get_state() != nullptr && !verify_state(data, get_state())) return false;
    if (verify_state(data, get_state_off())) publish(false);
    if (verify_state(data, get_state_on())) publish(true);
    state_data_ = data;
    publish(data);
    return true;
}

uint8_t UARTExDevice::get_state_data(uint32_t index)
{
    if (state_data_.size() <= index) return 0;
    return state_data_[index];
}

bool UARTExDevice::enqueue_tx_cmd(const cmd_t* cmd, bool low_priority)
{
    if (cmd == nullptr) return false;
    if (cmd->data.size() == 0) return false;
    if (low_priority) this->tx_cmd_queue_low_priority_.push(cmd);
    else this->tx_cmd_queue_.push(cmd);
    return true;
}

cmd_t* UARTExDevice::get_command(const std::string& name, const std::string& str)
{
    if (contains(this->command_str_func_map_, name))
    {
        this->command_map_[name] = (this->command_str_func_map_[name])(str);
        return &this->command_map_[name];
    }
    return get_command(name);
}

cmd_t* UARTExDevice::get_command(const std::string& name, const float x)
{
    if (contains(this->command_float_func_map_, name))
    {
        this->command_map_[name] = (this->command_float_func_map_[name])(x);
        return &this->command_map_[name];
    }
    return get_command(name);
}

cmd_t* UARTExDevice::get_command(const std::string& name)
{
    if (contains(this->command_func_map_, name))
    {
        this->command_map_[name] = (this->command_func_map_[name])();
        return &this->command_map_[name];
    }
    else if (contains(this->command_map_, name))
    {
        return &this->command_map_[name];
    }
    return nullptr;
}

state_t* UARTExDevice::get_state(const std::string& name)
{
    if (contains(this->state_map_, name)) return &this->state_map_[name];
    return nullptr;
}

optional<float> UARTExDevice::get_state_float(const std::string& name, const std::vector<uint8_t>& data)
{
    if (contains(this->state_float_func_map_, name)) return (this->state_float_func_map_[name])(&data[0], data.size());
    if (contains(this->state_num_map_, name)) return state_to_float(data, this->state_num_map_[name]);
    return optional<float>();
}

optional<std::string> UARTExDevice::get_state_str(const std::string& name, const std::vector<uint8_t>& data)
{
    if (contains(this->state_str_func_map_, name)) return (this->state_str_func_map_[name])(&data[0], data.size());
    return optional<std::string>();
}

bool UARTExDevice::has_state(const std::string& name)
{
    if (contains(this->state_float_func_map_, name)) return true;
    if (contains(this->state_str_func_map_, name)) return true;
    if (contains(this->state_num_map_, name)) return true;
    if (contains(this->state_map_, name)) return true;
    return false;
}

bool equal(const std::vector<uint8_t>& data1, const std::vector<uint8_t>& data2, const uint16_t offset)
{
    if (data1.size() - offset < data2.size()) return false;
    return std::equal(data1.begin() + offset, data1.begin() + offset + data2.size(), data2.begin());
}

const std::vector<uint8_t> masked_data(const std::vector<uint8_t>& data, const state_t* state)
{
    std::vector<uint8_t> masked_data = data;
    for (size_t i = state->offset, j = 0; i < data.size() && j < state->mask.size(); i++, j++)
    {
        masked_data[i] &= state->mask[j];
    }
    return masked_data;
}

bool verify_state(const std::vector<uint8_t>& data, const state_t* state)
{
    if (state == nullptr) return false;
    if (state->mask.size() == 0)    return equal(data, state->data, state->offset) ? !state->inverted : state->inverted;
    else                            return equal(masked_data(data, state), state->data, state->offset) ? !state->inverted : state->inverted;
    return false;
}

// float state_to_float(const std::vector<uint8_t>& data, const state_num_t state)
// {
//     int32_t val = 0;
//     for (uint16_t i = state.offset, len = 0; i < data.size() && len < state.length; i++, len++)
//     {
//         val = (val << 8) | (int8_t)data[i];
//     }
//     return val / powf(10, state.precision);
// }

float state_to_float(const std::vector<uint8_t>& data, const state_num_t state)
{
    uint32_t val = 0;
    std::string str;
    for (size_t i = 0; i < state.length && (state.offset + i) < data.size(); i++)
    {
        if (state.encode == ENCODE_BCD)
        {
            uint8_t byte = data[state.offset + i];
            uint8_t tens = (byte >> 4) & 0x0F;
            uint8_t ones = byte & 0x0F;
            if (tens > 9 || ones > 9) break;
            val = val * 100 + (tens * 10 + ones);
        }
        else if(state.encode == ENCODE_ASCII)
        {
            str.push_back((char)data[state.offset + i]);
        }
        else
        {
            if (state.endian == ENDIAN_BIG) val = (val << 8) | data[state.offset + i];
            else val |= static_cast<uint32_t>(data[state.offset + i]) << (8 * i);
        }
    }
    if(state.encode == ENCODE_ASCII) val = atoi(str.c_str());
    if (state.is_signed)
    {
        int shift = 32 - state.length * 8;
        int32_t signed_val = (static_cast<int32_t>(val) << shift) >> shift;
        return signed_val / powf(10, state.precision);
    }
    return val / powf(10, state.precision);
}

uint8_t float_to_bcd(const float val)
{
    int decimal_value = val;
    return ((decimal_value / 10) << 4) | (decimal_value % 10);
}

std::string to_hex_string(const std::vector<unsigned char>& data)
{
    char buf[10];
    std::string res;
    for (uint16_t i = 0; i < data.size(); i++)
    {
        sprintf(buf, "%02X", data[i]);
        res += buf;
    }
    sprintf(buf, "(%d)", data.size());
    res += buf;
    return res;
}

std::string to_ascii_string(const std::vector<unsigned char>& data)
{
    char buf[10];
    std::string res;
    for (uint16_t i = 0; i < data.size(); i++)
    {
        sprintf(buf, "%c", data[i]);
        res += buf;
    }
    sprintf(buf, "(%d)", data.size());
    res += buf;
    return res;
}

std::string to_hex_string(const uint8_t* data, const uint16_t len)
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