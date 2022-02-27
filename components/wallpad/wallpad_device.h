#pragma once
#include <vector>
#include <queue>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace wallpad {

static const char *TAG = "wallpad";
typedef unsigned short num_t;

/** State HEX Struct */
struct hex_t
{
    num_t offset;
    bool and_operator;
    bool inverted;
    std::vector<uint8_t> data;
};

/** Number state HEX Struct  **/
struct state_num_t
{
    num_t offset;
    num_t length;    // 1~4
    num_t precision; // 0~5
};

/** Command HEX Struct */
struct cmd_hex_t
{
    std::vector<uint8_t> data;
    std::vector<uint8_t> ack;
};

/**
 * WallPad Device
 */
class WallPadDevice : public PollingComponent
{
public:
    void update() override;
    void dump_wallpad_device_config(const char *TAG);

    void set_device(hex_t device) { device_ = device; }
    void set_sub_device(hex_t sub_device) { sub_device_ = sub_device; }
    void set_state_on(hex_t state_on) { state_on_ = state_on; }
    void set_state_off(hex_t state_off) { state_off_ = state_off; }

    void set_command_on(cmd_hex_t command_on) { command_on_ = command_on; }
    void set_command_on(std::function<cmd_hex_t()> command_on_func) { command_on_func_ = command_on_func; }
    const cmd_hex_t *get_command_on()
    {
        if (command_on_func_.has_value()) command_on_ = (*command_on_func_)();
        return &command_on_.value();
    }

    void set_command_off(cmd_hex_t command_off) { command_off_ = command_off; }
    void set_command_off(std::function<cmd_hex_t()> command_off_func) { command_off_func_ = command_off_func; }
    const cmd_hex_t *get_command_off()
    {
        if (command_off_func_.has_value()) command_off_ = (*command_off_func_)();
        return &command_off_.value();
    }

    void set_command_state(cmd_hex_t command_state) { command_state_ = command_state; }

    void send_command(const cmd_hex_t *cmd);
    bool is_have_command() { return tx_cmd_queue_.size() > 0 ? true : false; }
    const cmd_hex_t* get_command()
    {
        if (tx_cmd_queue_.size() == 0) return nullptr;
        const cmd_hex_t* cmd = tx_cmd_queue_.front();
        tx_cmd_queue_.pop();
        return cmd;
    }
    void ack_ok() { tx_cmd_queue_.size() == 0 ? set_tx_pending(false) : set_tx_pending(true); }
    void ack_ng() { ack_ok(); }
    void set_tx_pending(bool pending) { tx_pending_ = pending; }

    /** WallPad raw message parse */
    bool parse_data(const uint8_t *data, const num_t len);

    /** Publish other message from parse_date() */
    virtual void publish(const uint8_t *data, const num_t len) = 0;

    /** Publish on/off state message from parse_date() */
    virtual bool publish(bool state) = 0;

    /** priority of setup(). higher -> executed earlier */
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
    bool tx_pending_{false};
    std::queue<const cmd_hex_t*> tx_cmd_queue_{};
};


/** uint8_t[] to hex string  */
std::string hexencode(const uint8_t *raw_data, const num_t len);

/** uint8_t[] compare */
bool compare(const uint8_t *data1, const num_t len1, const uint8_t *data2, const num_t len2, const num_t offset);
bool compare(const uint8_t *data1, const num_t len1, const hex_t *data2);

/** uint8_t[] to decimal(float) */
float hex_to_float(const uint8_t *data, const num_t len, const num_t precision);

unsigned long elapsed_time(const unsigned long timer);
unsigned long set_time();

} // namespace wallpad
} // namespace esphome