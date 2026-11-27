#include "uartex_device.h"
#include <limits>

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
    if (this->state_debounce_ms_ > 0)
    {
        ESP_LOGCONFIG(TAG, "State Debounce: %ums", (unsigned) this->state_debounce_ms_);
        if (!this->debounce_mask_.empty())
        {
            ESP_LOGCONFIG(TAG, "State Debounce mask: %s, offset: %d", to_hex_string(this->debounce_mask_).c_str(), this->debounce_offset_);
        }
    }
    log_config(TAG, "Command On", get_command_on());
    log_config(TAG, "Command Off", get_command_off());
    log_config(TAG, "Command Update", get_command_update());
    if (get_command_update()) { LOG_UPDATE_INTERVAL(this); }
#endif
}

std::shared_ptr<const cmd_t> UARTExDevice::dequeue_tx_cmd()
{
    if (this->tx_cmd_queue_.empty()) return nullptr;
    if (get_state_response() && !this->rx_response_) return nullptr;
    this->rx_response_ = false;
    auto cmd = std::move(this->tx_cmd_queue_.front());
    this->tx_cmd_queue_.pop();
    return cmd;
}

std::shared_ptr<const cmd_t> UARTExDevice::dequeue_tx_cmd_low_priority()
{
    if (this->tx_cmd_queue_low_priority_.empty()) return nullptr;
    if (get_state_response() && !this->rx_response_) return nullptr;
    this->rx_response_ = false;
    auto cmd = std::move(this->tx_cmd_queue_low_priority_.front());
    this->tx_cmd_queue_low_priority_.pop();
    return cmd;
}

bool UARTExDevice::parse_data(const std::vector<uint8_t>& data)
{
    if (verify_state(data, get_state_response())) this->rx_response_ = true;
    else this->rx_response_ = false;
    if (get_state() != nullptr && !verify_state(data, get_state())) return false;
    if (this->state_debounce_ms_ == 0 || this->last_state_.empty())
    {
        // Debounce disabled, or the first state after boot: nothing to compare with.
        apply_state_(data);
        return true;
    }
    if (this->bypass_armed_ && static_cast<uint32_t>(millis() - this->bypass_since_) >= this->state_debounce_ms_)
        this->bypass_armed_ = false;  // window expired without a confirmation
    if (debounce_equal_(data, this->last_state_))
    {
        // Back to the applied state: the pending change was a glitch. Re-publish
        // so an entity changed by control()/optimistic is corrected immediately.
        this->pending_state_.clear();
        apply_state_(data);
        return true;
    }
    if (this->bypass_armed_)
    {
        // First change right after our own command: it is the confirmation, apply it now (one-shot).
        this->bypass_armed_ = false;
        this->pending_state_.clear();
        apply_state_(data);
        return true;
    }
    if (this->pending_state_.empty()) this->pending_since_ = millis();
    this->pending_state_ = data;
    return true;
}

// No mask: whole packets are compared. With a mask only the masked bits of
// bytes [offset, offset + mask.size()) are compared; a byte missing from both
// packets counts as equal, a byte present in only one of them as different.
bool UARTExDevice::debounce_equal_(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) const
{
    if (this->debounce_mask_.empty()) return a == b;
    for (size_t i = 0; i < this->debounce_mask_.size(); i++)
    {
        const size_t index = this->debounce_offset_ + i;
        const bool in_a = index < a.size(), in_b = index < b.size();
        if (in_a != in_b) return false;
        if (!in_a) continue;
        if ((a[index] & this->debounce_mask_[i]) != (b[index] & this->debounce_mask_[i])) return false;
    }
    return true;
}

void UARTExDevice::apply_state_(const std::vector<uint8_t>& data)
{
    if (verify_state(data, get_state_off())) publish(false);
    if (verify_state(data, get_state_on())) publish(true);
    publish(data);
    last_state_ = data;
    preserve_state_ready_ = true;
}

void UARTExDevice::arm_state_bypass(uint32_t now)
{
    if (this->state_debounce_ms_ == 0) return;
    this->bypass_armed_ = true;
    this->bypass_since_ = now;
}

void UARTExDevice::flush_pending_state(uint32_t now)
{
    // Disarm an expired bypass here too, so it cannot reappear after millis() wraps.
    if (this->bypass_armed_ && static_cast<uint32_t>(now - this->bypass_since_) >= this->state_debounce_ms_)
        this->bypass_armed_ = false;
    if (this->pending_state_.empty()) return;
    if (static_cast<uint32_t>(now - this->pending_since_) < this->state_debounce_ms_) return;
    std::vector<uint8_t> data = std::move(this->pending_state_);
    this->pending_state_.clear();
    apply_state_(data);
}

bool UARTExDevice::render_command(const cmd_t &input, cmd_t &output) const
{
    // While a debounced change is pending, last_state_ is stale: wait for it to settle.
    if (!input.preserve_state.empty() && (!preserve_state_ready_ || !pending_state_.empty() || state_map_.find("state") == state_map_.end())) return false;
    for (const auto offset : input.preserve_state)
        if (offset >= last_state_.size() || offset >= input.data.size()) return false;
    output = input;
    for (const auto offset : input.preserve_state)
        output.data[offset] = last_state_[offset];
    return true;
}

void UARTExDevice::invalidate_matching_state(const std::vector<uint8_t> &data)
{
    auto state = state_map_.find("state");
    if (state != state_map_.end() && verify_state(data, &state->second)) preserve_state_ready_ = false;
}

std::vector<uint8_t> UARTExDevice::last_state()
{
    return last_state_;
}

uint8_t UARTExDevice::last_state(const uint16_t index)
{
    if (index >= last_state_.size()) return 0;
    return last_state_[index];
}

bool UARTExDevice::enqueue_tx_cmd(const cmd_t* cmd, bool low_priority)
{
    if (cmd == nullptr) return false;
    if (cmd->data.empty()) return false;
    auto snapshot = std::make_shared<const cmd_t>(*cmd);
    if (low_priority) this->tx_cmd_queue_low_priority_.push(std::move(snapshot));
    else this->tx_cmd_queue_.push(std::move(snapshot));
    return true;
}

bool UARTExDevice::enqueue_and_reflect(const cmd_t* cmd)
{
    this->enqueue_tx_cmd(cmd);
    return this->optimistic_ && (cmd == nullptr || !cmd->data.empty());
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

bool UARTExDevice::has_named_command(const std::string& name)
{
    if (contains(this->command_func_map_, name)) return true;
    if (contains(this->command_float_func_map_, name)) return true;
    if (contains(this->command_str_func_map_, name)) return true;
    if (contains(this->command_map_, name)) return true;
    return false;
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
    // Guard against unsigned underflow when offset exceeds data1.size(): that
    // would make the size check pass and read out of bounds.
    if (offset > data1.size() || data1.size() - offset < data2.size()) return false;
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
    if (state->match == MATCH_EXACT)
    {
        if (data.size() != state->offset + state->data.size()) return false;
    }
    return equal(apply_mask(data, state), state->data, state->offset) ? !state->inverted : state->inverted;
}

float state_to_float(const std::vector<uint8_t>& data, const state_num_t& state)
{
    const size_t available = state.offset < data.size() ? data.size() - state.offset : 0;
    const size_t length = std::min(static_cast<size_t>(state.length), available);
    const auto byte_at = [&](size_t i) {
        return static_cast<uint8_t>(data[state.offset + i] & (i < state.mask.size() ? state.mask[i] : 0xFF));
    };
    if (state.decode == DECODE_ASCII)
    {
        std::string str;
        for (size_t i = 0; i < length; ++i) str.push_back(static_cast<char>(byte_at(i)));
        return static_cast<float>(atof(str.c_str())) / powf(10, state.precision);
    }

    double value = 0;
    if (state.decode == DECODE_BCD)
    {
        for (size_t i = 0; i < length; ++i)
        {
            uint8_t byte = byte_at(i);
            uint8_t tens = byte >> 4;
            uint8_t ones = byte & 0x0F;
            if (tens > 9 || ones > 9) break;
            value = value * 100 + tens * 10 + ones;
        }
    }
    else if (length > 0)
    {
        // Interpret signed binary as two's complement without integer shifts
        // or a subtract-after-rounding step that loses small negative values.
        const size_t high = state.endian == ENDIAN_BIG ? 0 : length - 1;
        if (state.is_signed && length == state.length && (byte_at(high) & 0x80)) value = -1;
        for (size_t i = 0; i < length; ++i)
        {
            const size_t index = state.endian == ENDIAN_BIG ? i : length - 1 - i;
            value = value * 256 + byte_at(index);
        }
    }
    value /= pow(10.0, state.precision);
    const double float_max = std::numeric_limits<float>::max();
    if (value > float_max) return std::numeric_limits<float>::infinity();
    if (value < -float_max) return -std::numeric_limits<float>::infinity();
    return static_cast<float>(value);
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
    if (len > 240) ascii_str.append("...");
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
    if (index >= tokens.size()) return default_val;
    return tokens[index];
}

bool check_value(const uint16_t index, const uint8_t value, const uint8_t* data, const uint16_t len)
{
    if (index >= len) return false;
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
    if (!state->mask.empty()) { ESP_LOGCONFIG(tag, "%s mask: %s", title, to_hex_string(state->mask).c_str()); }
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