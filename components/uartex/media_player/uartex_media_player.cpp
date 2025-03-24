#include "uartex_lock.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.media_player";

void UARTExMediaPlayer::dump_config()
{
    ESP_LOGCONFIG(TAG, "UARTEx Media Player '%s':", get_name().c_str());
    uartex_dump_config(TAG);
}

void UARTExMediaPlayer::setup()
{
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

media_player::MediaPlayerTraits UARTExMediaPlayer::get_traits()
{
    auto traits = media_player::MediaPlayerTraits();
    if (get_state_paused() || get_command_pause()) traits.set_supports_pause(true);
    return traits;
};

void UARTExMediaPlayer::publish(const std::vector<uint8_t>& data)
{
    bool changed = false;
    if (verify_state(data, get_state_none()))
    {
        this->state = media_player::MEDIA_PLAYER_STATE_NONE;
        changed = true;
    }
    else if (verify_state(data, get_state_idle()))
    {
        this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
        changed = true;
    }
    else if (verify_state(data, get_state_playing()))
    {
        this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        changed = true;
    }
    else if (verify_state(data, get_state_paused()))
    {
        this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
        changed = true;
    }
    else if (verify_state(data, get_state_announcing()))
    {
        this->state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
        changed = true;
    }
    optional<float> val = get_state_volume(data);
    if (val.has_value() && this->volume != val.value())
    {
        this->volume = val.value();
        changed = true;
    }
    if (changed) publish_state();
}

void UARTExMediaPlayer::control(const media_player::MediaPlayerCall &call)
{
    media_player::MediaPlayerState play_state = media_player::MEDIA_PLAYER_STATE_PLAYING;
    if (call.get_announcement().has_value())
    {
        if (call.get_announcement().value()) play_state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
    }

    this->is_announcement_ = false;
    if (play_state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING)
    {
        this->is_announcement_ = true;
    }

    // if (call.get_volume().has_value())
    // {
    //     this->volume = call.get_volume().value();
    // }
    if (call.get_command().has_value()) 

        switch (call.get_command().value())
        {
        case media_player::MEDIA_PLAYER_COMMAND_MUTE:
            if (!this->muted_) enqueue_tx_cmd(get_command_mute());
            this->muted_ = true;
            break;
        case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
            if (this->muted_) enqueue_tx_cmd(get_command_unmute());
            this->muted_ = false;
            break;
        case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP: 
        {
            float new_volume = this->volume + 0.1f;
            if (new_volume > 1.0f)
            new_volume = 1.0f;
            this->volume = new_volume;
            enqueue_tx_cmd(get_command_volume_up(new_volume));
            if (this->muted_) enqueue_tx_cmd(get_command_unmute());
            this->muted_ = false;
            break;
        }
        case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN: 
        {
            float new_volume = this->volume - 0.1f;
            if (new_volume < 0.0f)
            new_volume = 0.0f;
            this->volume = new_volume;
            enqueue_tx_cmd(get_command_volume_down(new_volume));
            if (this->muted_) enqueue_tx_cmd(get_command_unmute());
            this->muted_ = false;
            break;
        }

        case media_player::MEDIA_PLAYER_COMMAND_PLAY:
            this->state = play_state;
            enqueue_tx_cmd(get_command_play());
            break;
        case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
            this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
            enqueue_tx_cmd(get_command_pause());
            break;
        case media_player::MEDIA_PLAYER_COMMAND_STOP:
            this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
            enqueue_tx_cmd(get_command_stop());
            break;
        case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
            if (this->state == media_player::MEDIA_PLAYER_COMMAND_PLAY)
            {
                this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
                enqueue_tx_cmd(get_command_pause());
            } 
            else 
            {
                this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
                enqueue_tx_cmd(get_command_play());
            }
            enqueue_tx_cmd(get_command_toggle());
            break;
        case media_player::MEDIA_PLAYER_COMMAND_ENQUEUE :
            enqueue_tx_cmd(get_command_enqueue());
            break;
        case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE :
            enqueue_tx_cmd(get_command_repeat_one());
            break;
        case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF :
            enqueue_tx_cmd(get_command_repeat_off());
            break;
        case media_player::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST :
            enqueue_tx_cmd(get_command_clear_playlist());
            break;
    }
    this->publish_state();
}
}  // namespace uartex
}  // namespace esphome
