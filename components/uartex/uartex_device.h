#pragma once
#include <vector>
#include <queue>
#include <stdio.h>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

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
    void set_command_on(cmd_t command);
    void set_command_on(std::function<cmd_t(const uint8_t *state, const uint16_t len)> func);
    cmd_t* get_command_on();
    void set_command_off(cmd_t command);
    void set_command_off(std::function<cmd_t(const uint8_t *state, const uint16_t len)> func);
    cmd_t* get_command_off();
    void set_command_update(cmd_t command);
    void set_state_response(state_t state);
    void enqueue_tx_cmd(const cmd_t* cmd, bool low_priority = false);
    const cmd_t* dequeue_tx_cmd();
    const cmd_t* dequeue_tx_cmd_low_priority();
    bool parse_data(const std::vector<uint8_t>& data);

protected:
    float get_setup_priority() const override { return setup_priority::DATA; }
    virtual void publish(const std::vector<uint8_t>& data);
    virtual void publish(const bool state);
    
protected:
    optional<state_t> state_{};
    optional<state_t> state_on_{};
    optional<state_t> state_off_{};
    optional<cmd_t> command_on_{};
    optional<std::function<cmd_t(const uint8_t *data, const uint16_t len)>> command_on_func_{};
    optional<cmd_t> command_off_{};
    optional<std::function<cmd_t(const uint8_t *data, const uint16_t len)>> command_off_func_{};
    optional<cmd_t> command_update_;
    optional<state_t> state_response_{};
    bool rx_response_{false};
    std::queue<const cmd_t*> tx_cmd_queue_{};
    std::queue<const cmd_t*> tx_cmd_queue_low_priority_{};
    std::vector<uint8_t> last_state_{};
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