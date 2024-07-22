#pragma once
#include <vector>
#include <queue>
#include <stdio.h>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <unordered_map>

namespace esphome {
namespace uartex {

struct state_t
{
    uint16_t offset;
    bool inverted;
    std::vector<uint8_t> data;
    std::vector<uint8_t> mask;
};

struct state_num_t
{
    uint16_t offset;
    uint16_t length;    // 1~4
    uint16_t precision; // 0~5
};

struct cmd_t
{
    std::vector<uint8_t> data;
    std::vector<uint8_t> ack;
};

class UARTExDevice : public PollingComponent
{
public:
    void update() override;
    void uartex_dump_config(const char* TAG);
    void set_state(std::string name, state_t state) { this->state_map_[name] = state; }
    void set_state(std::string name, state_num_t state) { this->state_num_map_[name] = state; }
    void set_state(std::string name, std::function<float(const uint8_t* data, const uint16_t len)> f) { this->state_float_func_map_[name] = f; }
    void set_state(std::string name, std::function<const char*(const uint8_t* data, const uint16_t len)> f) { this->state_str_func_map_[name] = f; }
    void set_command(std::string name, cmd_t cmd) { this->command_map_[name] = cmd; }
    void set_command(std::string name, std::function<cmd_t()> f) { this->command_func_map_[name] = f; }
    void set_command(std::string name, std::function<cmd_t(const float x)> f) { this->command_float_func_map_[name] = f; }
    void set_command(std::string name, std::function<cmd_t(const std::string& str)> f) { this->command_str_func_map_[name] = f; }

    void enqueue_tx_cmd(const cmd_t* cmd, bool low_priority = false);
    const cmd_t* dequeue_tx_cmd();
    const cmd_t* dequeue_tx_cmd_low_priority();
    bool parse_data(const std::vector<uint8_t>& data);

protected:
    float get_setup_priority() const override { return setup_priority::DATA; }
    virtual void publish(const std::vector<uint8_t>& data) {}
    virtual void publish(const bool state) {}
    cmd_t* get_command(const std::string& name, const std::string& str);
    cmd_t* get_command(const std::string& name, const float x);
    cmd_t* get_command(const std::string& name);
    state_t* get_state(const std::string& name);
    optional<float> get_state_float(const std::string& name, const std::vector<uint8_t>& data);
    optional<const char*> get_state_str(const std::string& name, const std::vector<uint8_t>& data);
    bool has_state(const std::string& name);
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
    std::unordered_map<std::string, std::function<const char*(const uint8_t* data, const uint16_t len)>> state_str_func_map_{};

    std::unordered_map<std::string, cmd_t> command_map_{};
    std::unordered_map<std::string, std::function<cmd_t()>> command_func_map_{};
    std::unordered_map<std::string, std::function<cmd_t(const float x)>> command_float_func_map_{};
    std::unordered_map<std::string, std::function<cmd_t(const std::string& str)>> command_str_func_map_{};
    
    bool rx_response_{false};
    std::queue<const cmd_t*> tx_cmd_queue_{};
    std::queue<const cmd_t*> tx_cmd_queue_low_priority_{};
};

template<typename KeyType, typename ValueType>
bool contains(const std::unordered_map<KeyType, ValueType>& map, const KeyType& key) { return map.find(key) != map.end(); }
bool equal(const std::vector<uint8_t>& data1, const std::vector<uint8_t>& data2,  const uint16_t offset = 0);
const std::vector<uint8_t> masked_data(const std::vector<uint8_t>& data, const state_t* state);
bool verify_state(const std::vector<uint8_t>& data, const state_t* state);
float state_to_float(const std::vector<uint8_t>& data, const state_num_t state);
std::string to_hex_string(const std::vector<unsigned char>& data);
std::string to_hex_string(const uint8_t* data, const uint16_t len);
unsigned long elapsed_time(const unsigned long timer);
unsigned long get_time();

} // namespace uartex
} // namespace esphome