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

/**
 * UARTEx Device
 */
class UARTExDevice : public PollingComponent
{
public:
    void update() override;
    void dump_uartex_device_config(const char *TAG);
    void set_state(state_t state);
    void set_state_on(state_t state);
    void set_state_off(state_t state);
    void set_state_response(state_t state);
    void set_command_on(cmd_t cmd);
    void set_command_on(std::function<cmd_t()> f);
    void set_command_off(cmd_t cmd);
    void set_command_off(std::function<cmd_t()> f);
    void set_command_update(cmd_t cmd);
    void set_command_update(std::function<cmd_t()> f);

    state_t* get_state();
    state_t* get_state_on();
    state_t* get_state_off();
    state_t* get_state_response();
    optional<float> get_state_func(std::string name, const uint8_t *data, const uint16_t len);
    cmd_t* get_command_on();
    cmd_t* get_command_off();
    cmd_t* get_command_update();
    bool has_state_func(std::string name);
    void enqueue_tx_cmd(const cmd_t* cmd, bool low_priority = false);
    const cmd_t* dequeue_tx_cmd();
    const cmd_t* dequeue_tx_cmd_low_priority();
    bool parse_data(const std::vector<uint8_t>& data);

protected:
    float get_setup_priority() const override { return setup_priority::DATA; }
    virtual void publish(const std::vector<uint8_t>& data);
    virtual void publish(const bool state);
    cmd_t* get_command(std::string name);
    state_t* get_state(std::string name);
    state_num_t* get_state_num(std::string name);
    
protected:

    std::unordered_map<std::string, state_t> state_map_{};
    std::unordered_map<std::string, state_num_t> state_num_map_{};
    std::unordered_map<std::string, std::function<optional<float>(const uint8_t *data, const uint16_t len)>> state_func_map_{};
    std::unordered_map<std::string, std::function<cmd_t(const float x)>> command_param_func_map_{};
    std::unordered_map<std::string, std::function<cmd_t()>> command_func_map_{};
    std::unordered_map<std::string, cmd_t> command_map_{};

    bool rx_response_{false};
    std::queue<const cmd_t*> tx_cmd_queue_{};
    std::queue<const cmd_t*> tx_cmd_queue_low_priority_{};
};

bool equal(const std::vector<uint8_t>& data1, const std::vector<uint8_t>& data2,  const uint16_t offset = 0);
const std::vector<uint8_t> masked_data(const std::vector<uint8_t> &data, const state_t *state);
bool verify_state(const std::vector<uint8_t>& data, const state_t *state);
float state_to_float(const std::vector<uint8_t>& data, const state_num_t state);
std::string to_hex_string(const std::vector<unsigned char>& data);
std::string to_hex_string(const uint8_t *data, const uint16_t len);
unsigned long elapsed_time(const unsigned long timer);
unsigned long get_time();

} // namespace uartex
} // namespace esphome