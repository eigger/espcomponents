#pragma once
#include "esphome/components/uartex/uartex_device.h"
#include "esphome/components/media_player/media_player.h"

namespace esphome {
namespace uartex {

class UARTExMediaPlayer : public media_player::MediaPlayer, public UARTExDevice 
{
public:
    void dump_config() override;
    void setup() override;

    state_t* get_state_none() { return get_state("state_none"); }
    state_t* get_state_idle() { return get_state("state_idle"); }
    state_t* get_state_playing() { return get_state("state_playing"); }
    state_t* get_state_paused() { return get_state("state_paused"); }
    state_t* get_state_announcing() { return get_state("state_announcing"); }
    optional<float> get_state_volume(const std::vector<uint8_t>& data) { return get_state_float("state_volume", data); }

    cmd_t* get_command_stop() { return get_command("command_stop"); }
    cmd_t* get_command_play() { return get_command("command_play"); }
    cmd_t* get_command_pause() { return get_command("command_pause"); }
    cmd_t* get_command_mute() { return get_command("command_mute"); }
    cmd_t* get_command_unmute() { return get_command("command_unmute"); }
    cmd_t* get_command_toggle() { return get_command("command_toggle"); }
    cmd_t* get_command_volume(const float x) { return get_command("command_volume", x); }
    cmd_t* get_command_volume_up(const float x) { return get_command("command_volume_up", x); }
    cmd_t* get_command_volume_down(const float x) { return get_command("command_volume_down", x); }
    cmd_t* get_command_enqueue() { return get_command("command_enqueue"); }
    cmd_t* get_command_repeat_one() { return get_command("command_repeat_one"); }
    cmd_t* get_command_repeat_off() { return get_command("command_repeat_off"); }
    cmd_t* get_command_clear_playlist() { return get_command("command_clear_playlist"); }
protected:
    media_player::MediaPlayerTraits get_traits() override;
    bool is_muted() const override { return this->muted_; }
    void publish(const std::vector<uint8_t>& data) override;
    void control(const media_player::MediaPlayerCall &call) override;
    
protected:
    bool muted_{false};
    float unmuted_volume_{0};
    bool is_announcement_{false};
};

}  // namespace uartex
}  // namespace esphome