#pragma once
#include "define.h"
#include "wallpad_listener.h"

namespace esphome {
namespace wallpad {


/**
 * WallPad Device
 */
class WallPadDevice : public WallPadListener, public PollingComponent
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

    void write_with_header(const cmd_hex_t *cmd);
    void callback() { tx_pending_ = false; }

    /** WallPad raw message parse */
    bool parse_data(const uint8_t *data, const num_t len) override;

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
};



}
}