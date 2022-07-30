#pragma once
#include <vector>
#include <queue>
#include <stdio.h>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace uartex {

typedef unsigned short num_t;

struct hex_t
{
    num_t offset;
    bool and_operator;
    bool inverted;
    std::vector<uint8_t> data;
};

struct state_num_t
{
    num_t offset;
    num_t length;    // 1~4
    num_t precision; // 0~5
};

struct cmd_hex_t
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
    void set_device(hex_t device);
    void set_sub_device(hex_t sub_device);
    void set_state_on(hex_t state_on);
    void set_state_off(hex_t state_off);
    void set_command_on(cmd_hex_t command_on);
    void set_command_on(std::function<cmd_hex_t()> command_on_func);
    const cmd_hex_t* get_command_on();
    void set_command_off(cmd_hex_t command_off);
    void set_command_off(std::function<cmd_hex_t()> command_off_func);
    const cmd_hex_t* get_command_off();
    void set_command_state(cmd_hex_t command_state);
    void set_state_response(hex_t state_response);
    void push_tx_cmd(const cmd_hex_t* cmd);
    const cmd_hex_t* pop_tx_cmd();
    void ack_ok();
    void ack_ng();
    bool equal(const std::vector<uint8_t>& data1, const std::vector<uint8_t>& data2,  const num_t offset = 0);
    bool validate(const std::vector<uint8_t>& data, const hex_t *cmd);
    float state_to_float(const std::vector<uint8_t>& data, const state_num_t state);

    bool parse_data(const std::vector<uint8_t>& data);
    virtual void publish(const std::vector<uint8_t>& data) = 0;
    virtual bool publish(bool state) = 0;
    float get_setup_priority() const override { return setup_priority::DATA; }

protected:
    const std::string *device_name_;
    hex_t device_{};
    optional<hex_t> sub_device_{};
    optional<hex_t> state_on_{};
    optional<hex_t> state_off_{};
    optional<cmd_hex_t> command_on_{};
    optional<std::function<cmd_hex_t()>> command_on_func_{};
    optional<cmd_hex_t> command_off_{};
    optional<std::function<cmd_hex_t()>> command_off_func_{};
    optional<cmd_hex_t> command_state_;
    optional<hex_t> state_response_{};
    bool rx_response_{false};
    std::queue<const cmd_hex_t*> tx_cmd_queue_{};
};

std::string to_hex_string(const std::vector<unsigned char>& data);

} // namespace uartex
} // namespace esphome