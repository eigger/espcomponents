#pragma once
#include <vector>
#include <queue>
#include <string>
#include <cstdint>
#include <cstdio>
#include <unordered_map>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/core/application.h"


namespace esphome {
namespace uartex {

enum ENDIAN {
    ENDIAN_BIG,
    ENDIAN_LITTLE
};

enum DECODE {
    DECODE_NONE,
    DECODE_BCD,
    DECODE_ASCII
};


struct state_t
{
    std::vector<uint8_t> data;
    std::vector<uint8_t> mask;
    uint16_t offset;
    bool inverted;
    state_t() = default;
    state_t(
        std::initializer_list<uint8_t> data, 
        std::initializer_list<uint8_t> mask = {}, 
        uint16_t offset = 0, 
        bool inverted = false)
    : data(data), mask(mask), offset(offset), inverted(inverted) {}
};

struct state_num_t
{
    uint16_t offset;
    uint16_t length;    // 1~4
    uint16_t precision; // 0~5
    bool is_signed;
    ENDIAN endian;
    DECODE decode;
    state_num_t() = default;
    state_num_t(
        uint16_t offset, 
        uint16_t length = 1, 
        uint16_t precision = 0, 
        bool is_signed = true, 
        ENDIAN endian = ENDIAN_BIG, 
        DECODE decode = DECODE_NONE)
    : offset(offset), length(length), precision(precision), is_signed(is_signed), endian(endian), decode(decode) {}
};

struct cmd_t
{
    std::vector<uint8_t> data;
    std::vector<uint8_t> ack;
    std::vector<uint8_t> mask;

    cmd_t() = default;
    cmd_t(
        std::initializer_list<uint8_t> data, 
        std::initializer_list<uint8_t> ack = {}, 
        std::initializer_list<uint8_t> mask = {})
    : data(data), ack(ack), mask(mask) {}
    cmd_t(
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& ack = {},
        const std::vector<uint8_t>& mask = {})
    : data(data), ack(ack), mask(mask) {}
    cmd_t(
        const std::string& data_str,
        const std::string& ack_str = {},
        const std::string& mask_str = {})
    : data(data_str.begin(), data_str.end()), ack(ack_str.begin(), ack_str.end()), mask(mask_str.begin(), mask_str.end()) {}
};

class UARTExDevice : public PollingComponent
{
public:
    void update() override;
    void uartex_dump_config(const char* TAG);
    void set_state(std::string name, state_t state) { this->state_map_[name] = state; }
    void set_state(std::string name, state_num_t state) { this->state_num_map_[name] = state; }
    void set_state(std::string name, std::function<float(const uint8_t* data, const uint16_t len)> &&f) { this->state_float_func_map_[name] = f; }
    void set_state(std::string name, std::function<std::string(const uint8_t* data, const uint16_t len)> &&f) { this->state_str_func_map_[name] = f; }
    void set_command(std::string name, cmd_t cmd) { this->command_map_[name] = cmd; }
    void set_command(std::string name, std::function<cmd_t()> &&f) { this->command_func_map_[name] = f; }
    void set_command(std::string name, std::function<cmd_t(const float x)> &&f) { this->command_float_func_map_[name] = f; }
    void set_command(std::string name, std::function<cmd_t(const std::string& str)> &&f) { this->command_str_func_map_[name] = f; }
    void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
    bool enqueue_tx_cmd(const cmd_t* cmd, bool low_priority = false);
    const cmd_t* dequeue_tx_cmd();
    const cmd_t* dequeue_tx_cmd_low_priority();
    bool parse_data(const std::vector<uint8_t>& data);
    std::vector<uint8_t> last_state();
    uint8_t last_state(const uint16_t index);
protected:
    float get_setup_priority() const override { return setup_priority::DATA; }
    virtual void publish(const std::vector<uint8_t>& data) {}
    virtual void publish(const bool state) {}
    cmd_t* get_command(const std::string& name, const std::string& str);
    cmd_t* get_command(const std::string& name, const float x);
    cmd_t* get_command(const std::string& name);
    state_t* get_state(const std::string& name);
    state_num_t* get_state_num(const std::string& name);
    optional<float> get_state_float(const std::string& name, const std::vector<uint8_t>& data);
    optional<std::string> get_state_str(const std::string& name, const std::vector<uint8_t>& data);
    bool has_named_state(const std::string& name);
    state_t* get_state() { return get_state("state"); }
    state_t* get_state_on() { return get_state("state_on"); }
    state_t* get_state_off() { return get_state("state_off"); }
    state_t* get_state_response() { return get_state("state_response"); }
    cmd_t* get_command_on() { return get_command("command_on"); }
    cmd_t* get_command_off() { return get_command("command_off"); }
    cmd_t* get_command_update() { return get_command("command_update"); }

protected:
    std::unordered_map<std::string, state_t> state_map_{};
    std::unordered_map<std::string, state_num_t> state_num_map_{};
    std::unordered_map<std::string, std::function<float(const uint8_t* data, const uint16_t len)>> state_float_func_map_{};
    std::unordered_map<std::string, std::function<std::string(const uint8_t* data, const uint16_t len)>> state_str_func_map_{};

    std::unordered_map<std::string, cmd_t> command_map_{};
    std::unordered_map<std::string, std::function<cmd_t()>> command_func_map_{};
    std::unordered_map<std::string, std::function<cmd_t(const float x)>> command_float_func_map_{};
    std::unordered_map<std::string, std::function<cmd_t(const std::string& str)>> command_str_func_map_{};
    
    bool rx_response_{false};
    bool optimistic_{false};
    std::queue<const cmd_t*> tx_cmd_queue_{};
    std::queue<const cmd_t*> tx_cmd_queue_low_priority_{};
    std::vector<uint8_t> last_state_{};
};
const char* find_mode(const std::vector<const char*>& modes, const std::string& target);
template<typename KeyType, typename ValueType>
bool contains(const std::unordered_map<KeyType, ValueType>& map, const KeyType& key) { return map.find(key) != map.end(); }
bool equal(const std::vector<uint8_t>& data1, const std::vector<uint8_t>& data2,  const uint16_t offset = 0);
bool equal_cmd(const cmd_t& a, const cmd_t& b);
std::vector<uint8_t> apply_mask(const std::vector<uint8_t>& data, const state_t* state);
bool verify_state(const std::vector<uint8_t>& data, const state_t* state);
float state_to_float(const std::vector<uint8_t>& data, const state_num_t state);
uint8_t float_to_bcd(const float val);
std::string to_hex_string(const std::vector<unsigned char>& data);
std::string to_ascii_string(const std::vector<unsigned char>& data);
std::string to_hex_string(const uint8_t* data, const uint16_t len);
std::string to_ascii_string(const uint8_t* data, const uint16_t len);
std::vector<std::string> split(const std::string& str, const std::string& delimiter = ",");
std::string get_token(const std::vector<std::string>& tokens, size_t index, const std::string& default_val = "");
bool check_value(const uint16_t index, const uint8_t value, const uint8_t* data, const uint16_t len);
uint16_t crc16(const uint16_t init, const uint16_t poly, const uint8_t data);
uint16_t crc16_reflected(const uint16_t init, const uint16_t poly, const uint8_t data);
std::vector<uint8_t> crc16_checksum(const uint16_t init, const uint16_t poly, const uint8_t* data, const uint16_t len);
std::vector<uint8_t> crc16_reflected_checksum(const uint16_t init, const uint16_t poly, const uint8_t* data, const uint16_t len);
unsigned long elapsed_time(const unsigned long timer);
unsigned long get_time();
void log_config(const char* tag, const char* title, const char* value);
void log_config(const char* tag, const char* title, const uint16_t value);
void log_config(const char* tag, const char* title, const bool value);
void log_config(const char* tag, const char* title, const std::vector<uint8_t>& value);
void log_config(const char* tag, const char* title, const state_t* state);
void log_config(const char* tag, const char* title, const state_num_t* state_num);
void log_config(const char* tag, const char* title, const cmd_t* cmd);
} // namespace uartex
} // namespace esphome