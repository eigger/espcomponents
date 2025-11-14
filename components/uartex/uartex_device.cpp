#include "uartex_device.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex";

void UARTExDevice::update()
{
    enqueue_tx_cmd(get_command_update(), true);
}

void UARTExDevice::uartex_dump_config(const char* TAG)
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "State", get_state());
    log_config(TAG, "State On", get_state_on());
    log_config(TAG, "State Off", get_state_off());
    log_config(TAG, "State Response", get_state_response());
    log_config(TAG, "Command On", get_command_on());
    log_config(TAG, "Command Off", get_command_off());
    log_config(TAG, "Command Update", get_command_update());
    if (get_command_update()) { LOG_UPDATE_INTERVAL(this); }
#endif
}

const cmd_t *UARTExDevice::dequeue_tx_cmd()
{
    if (get_state_response() && !this->rx_response_) return nullptr;
    this->rx_response_ = false;
    if (this->tx_cmd_queue_.empty()) return nullptr;
    const cmd_t *cmd = this->tx_cmd_queue_.front();
    this->tx_cmd_queue_.pop();
    return cmd;
}

const cmd_t *UARTExDevice::dequeue_tx_cmd_low_priority()
{
    if (get_state_response() && !this->rx_response_) return nullptr;
    this->rx_response_ = false;
    if (this->tx_cmd_queue_low_priority_.empty()) return nullptr;
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
    publish(data);
    last_state_ = data;
    return true;
}

std::vector<uint8_t> UARTExDevice::last_state()
{
    return last_state_;
}

uint8_t UARTExDevice::last_state(const uint16_t index)
{
    if (index < 0 || index >= last_state_.size()) return 0;
    return last_state_[index];
}

bool UARTExDevice::enqueue_tx_cmd(const cmd_t* cmd, bool low_priority)
{
    if (cmd == nullptr) return false;
    if (cmd->data.empty()) return false;
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

state_num_t* UARTExDevice::get_state_num(const std::string& name)
{
    if (contains(this->state_num_map_, name)) return &this->state_num_map_[name];
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

bool UARTExDevice::has_named_state(const std::string& name)
{
    if (contains(this->state_float_func_map_, name)) return true;
    if (contains(this->state_str_func_map_, name)) return true;
    if (contains(this->state_num_map_, name)) return true;
    if (contains(this->state_map_, name)) return true;
    return false;
}

const char* find_mode(const std::vector<const char*>& modes, const std::string& target)
{
    auto it = std::find_if(modes.begin(), modes.end(), [&](const char* m)
        {
            return m != nullptr && target == m;
        }
    );
    if (it != modes.end()) {
        return *it;
    }
    return nullptr;
}

bool equal(const std::vector<uint8_t>& data1, const std::vector<uint8_t>& data2, const uint16_t offset)
{
    if (data1.size() - offset < data2.size()) return false;
    return std::equal(data1.begin() + offset, data1.begin() + offset + data2.size(), data2.begin());
}

bool equal_cmd(const cmd_t& a, const cmd_t& b)
{
    return a.data == b.data
        && a.ack  == b.ack
        && a.mask == b.mask;
}

std::vector<uint8_t> apply_mask(const std::vector<uint8_t>& data, const state_t* state)
{
    if (state == nullptr || state->mask.empty()) return data;
    std::vector<uint8_t> masked_data = data;
    for (size_t i = 0; (state->offset + i) < data.size() && i < state->mask.size(); i++)
    {
        masked_data[state->offset + i] &= state->mask[i];
    }
    return masked_data;
}

bool verify_state(const std::vector<uint8_t>& data, const state_t* state)
{
    if (state == nullptr) return false;
    return equal(apply_mask(data, state), state->data, state->offset) ? !state->inverted : state->inverted;
}

float state_to_float(const std::vector<uint8_t>& data, const state_num_t state)
{
    uint32_t val = 0;
    std::string str;
    uint16_t byte_length = state.length < 4 ? state.length : 4;
    uint16_t length = state.length;
    if (state.decode != DECODE_ASCII && length > 4) length = 4;
    for (size_t i = 0; i < length && (state.offset + i) < data.size(); i++)
    {
        if (state.decode == DECODE_BCD)
        {
            uint8_t byte = data[state.offset + i];
            uint8_t tens = (byte >> 4) & 0x0F;
            uint8_t ones = byte & 0x0F;
            if (tens > 9 || ones > 9) break;
            val = val * 100 + (tens * 10 + ones);
        }
        else if(state.decode == DECODE_ASCII)
        {
            str.push_back((char)data[state.offset + i]);
        }
        else
        {
            if (state.endian == ENDIAN_BIG) val = (val << 8) | data[state.offset + i];
            else val |= static_cast<uint32_t>(data[state.offset + i]) << (8 * i);
        }
    }
    if (state.decode == DECODE_ASCII)
    {
        return (float)atof(str.c_str()) / powf(10, state.precision);
    } 
    if (state.is_signed)
    {
        int shift = 32 - byte_length * 8;
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
    return to_hex_string(&data[0], data.size());
}

std::string to_ascii_string(const std::vector<unsigned char>& data)
{
    return to_ascii_string(&data[0], data.size());
}

std::string to_hex_string(const uint8_t* data, const uint16_t len)
{
    char buf[3] = {0}; 
    std::string hex_str;
    uint16_t size = len >= 120 ? 120 : len;
    hex_str.reserve(static_cast<size_t>(size) * 2 + 10);
    for (uint16_t i = 0; i < size; ++i)
    {
        std::snprintf(buf, sizeof(buf), "%02X", data[i]);
        hex_str.append(buf);
    }
    if (len > 120) hex_str.append("...");
    char size_buf[16] = {0};
    std::snprintf(size_buf, sizeof(size_buf), "(%u)", len);
    hex_str.append(size_buf);
    return hex_str;
}

std::string to_ascii_string(const uint8_t* data, const uint16_t len)
{
    char buf[2] = {0}; 
    std::string ascii_str;
    uint16_t size = len >= 240 ? 240 : len;
    ascii_str.reserve(static_cast<size_t>(size) + 10);
    for (uint16_t i = 0; i < size; ++i)
    {
        std::snprintf(buf, sizeof(buf), "%c", data[i]);
        ascii_str.append(buf);
    }
    if (len > 120) ascii_str.append("...");
    char size_buf[16] = {0};
    std::snprintf(size_buf, sizeof(size_buf), "(%u)", len);
    ascii_str.append(size_buf);
    return ascii_str;
}

std::vector<std::string> split(const std::string& str, const std::string& delimiter)
{
    std::vector<std::string> tokens;
    size_t pos_start = 0, pos;
    size_t delim_len = delimiter.length();
    while ((pos = str.find(delimiter, pos_start)) != std::string::npos)
    {
        tokens.emplace_back(str.substr(pos_start, pos - pos_start));
        pos_start = pos + delim_len;
    }
    tokens.emplace_back(str.substr(pos_start));
    return tokens;
}

std::string get_token(const std::vector<std::string>& tokens, size_t index, const std::string& default_val) 
{
    if (index < 0 || index >= tokens.size()) return default_val;
    return tokens[index];
}

bool check_value(const uint16_t index, const uint8_t value, const uint8_t* data, const uint16_t len)
{
    if (index < 0 || index >= len) return false;
    return data[index] == value;
}

uint16_t crc16(const uint16_t init, const uint16_t poly, const uint8_t data)
{
    uint16_t crc = init;
    crc ^= ((uint16_t)data << 8);
    for (int i = 0; i < 8; i++)
    {
        if (crc & 0x8000)
        {
            crc = (crc << 1) ^ poly;
        } 
        else
        {
            crc = crc << 1;
        }
    }
    return crc;
}

uint16_t crc16_reflected(const uint16_t init, const uint16_t poly, const uint8_t data)
{
    uint16_t crc = init;
    crc ^= data;
    for (int i = 0; i < 8; i++)
    {
        if (crc & 0x0001)
        {
            crc = (crc >> 1) ^ poly;
        } 
        else
        {
            crc = crc >> 1;
        }
    }
    return crc;
}

std::vector<uint8_t> crc16_checksum(const uint16_t init, const uint16_t poly, const uint8_t* data, const uint16_t len)
{
    uint16_t crc = init;
    for (int i = 0; i < len; i++)
    {
        crc = crc16(crc, poly, data[i]);
    }
    return { (uint8_t)(crc >> 8), (uint8_t)(crc) };
}

std::vector<uint8_t> crc16_reflected_checksum(const uint16_t init, const uint16_t poly, const uint8_t* data, const uint16_t len)
{
    uint16_t crc = init;
    for (int i = 0; i < len; i++)
    {
        crc = crc16_reflected(crc, poly, data[i]);
    }
    return { (uint8_t)(crc >> 8), (uint8_t)(crc) };
}

unsigned long elapsed_time(const unsigned long timer)
{
    return get_time() - timer;
}

unsigned long get_time()
{
    return millis();
    //return App.get_loop_component_start_time();
}

void log_config(const char* tag, const char* title, const char* value)
{
    if (value == 0) return;
    ESP_LOGCONFIG(tag, "%s: %s", title, value);
}

void log_config(const char* tag, const char* title, const uint16_t value)
{
    if (value == 0) return;
    ESP_LOGCONFIG(tag, "%s: %d", title, value);
}

void log_config(const char* tag, const char* title, const bool value)
{
    if (!value) return;
    ESP_LOGCONFIG(tag, "%s: %s", title, YESNO(value));
}

void log_config(const char* tag, const char* title, const std::vector<uint8_t>& value)
{
    if (value.empty()) return;
    ESP_LOGCONFIG(tag, "%s: %s", title, to_hex_string(value).c_str());
}

void log_config(const char* tag, const char* title, const state_t* state)
{
    if (state == nullptr) return;
    ESP_LOGCONFIG(tag, "%s: %s, offset: %d, inverted: %s", title, to_hex_string(state->data).c_str(), state->offset, YESNO(state->inverted));
    if (!state->mask.empty()) ESP_LOGCONFIG(tag, "%s mask: %s", title, to_hex_string(state->mask).c_str());
}

void log_config(const char* tag, const char* title, const state_num_t* state_num)
{
    if (state_num == nullptr) return;
    ESP_LOGCONFIG(tag, "%s: offset: %d, length: %d, precision: %d", title, state_num->offset, state_num->length, state_num->precision);
    ESP_LOGCONFIG(tag, "%s: signed: %s, endian: %d, decode: %d", title, YESNO(state_num->is_signed), state_num->endian, state_num->decode);
}

void log_config(const char* tag, const char* title, const cmd_t* cmd)
{
    if (cmd == nullptr) return;
    ESP_LOGCONFIG(tag, "%s: %s, Ack: %s, Mask: %s", title, to_hex_string(cmd->data).c_str(), to_hex_string(cmd->ack).c_str(), to_hex_string(cmd->mask).c_str());
}

}  // namespace uartex
}  // namespace esphome