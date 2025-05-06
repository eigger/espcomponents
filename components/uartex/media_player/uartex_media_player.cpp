#include "uartex_media_player.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.media_player";

void UARTExMediaPlayer::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    log_config(TAG, "State None", get_state_none());
    log_config(TAG, "State Idle", get_state_idle());
    log_config(TAG, "State Playing", get_state_playing());
    log_config(TAG, "State Paused", get_state_paused());
    log_config(TAG, "State Announcing", get_state_announcing());
    log_config(TAG, "State Volume", get_state_num("state_volume"));
    log_config(TAG, "Command Stop", get_command_stop());
    log_config(TAG, "Command Play", get_command_play());
    log_config(TAG, "Command Pause", get_command_pause());
    log_config(TAG, "Command Mute", get_command_mute());
    log_config(TAG, "Command Unmute", get_command_unmute());
    log_config(TAG, "Command Toggle", get_command_toggle());
    log_config(TAG, "Command Enqueue", get_command_enqueue());
    log_config(TAG, "Command Repeat One", get_command_repeat_one());
    log_config(TAG, "Command Repeat Off", get_command_repeat_off());
    log_config(TAG, "Command Clear Playlist", get_command_clear_playlist());
    uartex_dump_config(TAG);
#endif
}

void UARTExMediaPlayer::setup()
{
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

media_player::MediaPlayerTraits UARTExMediaPlayer::get_traits()
{
    auto traits = media_player::MediaPlayerTraits();
    //if (get_state_paused() || get_command_pause())
    traits.set_supports_pause(true);
    return traits;
}

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
    float new_volume = this->volume;
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

    if (call.get_volume().has_value())
    {
        float volume = call.get_volume().value();
        if (enqueue_tx_cmd(get_command_volume(volume * 100)) || this->optimistic_)
        {
            this->volume = volume;
        }
        if (this->muted_) enqueue_tx_cmd(get_command_unmute());
        this->muted_ = false;
    }
    if (call.get_command().has_value()) 
    {
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
            new_volume = this->volume + 0.1f;
            if (new_volume > 1.0f) new_volume = 1.0f;
            if (enqueue_tx_cmd(get_command_volume_up(new_volume * 100)) || this->optimistic_)
            {
                this->volume = new_volume;
            }
            if (this->muted_) enqueue_tx_cmd(get_command_unmute());
            this->muted_ = false;
            break;
        case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN: 
            new_volume = this->volume - 0.1f;
            if (new_volume < 0.0f) new_volume = 0.0f;
            if (enqueue_tx_cmd(get_command_volume_down(new_volume * 100)) || this->optimistic_)
            {
                this->volume = new_volume;
            }
            if (this->muted_) enqueue_tx_cmd(get_command_unmute());
            this->muted_ = false;
            break;
        case media_player::MEDIA_PLAYER_COMMAND_PLAY:
            if (enqueue_tx_cmd(get_command_play()) || this->optimistic_)
            {
                this->state = play_state;
            }
            break;
        case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
            if (enqueue_tx_cmd(get_command_pause()) || this->optimistic_)
            {
                this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
            }
            break;
        case media_player::MEDIA_PLAYER_COMMAND_STOP:
            if (enqueue_tx_cmd(get_command_stop()) || this->optimistic_)
            {
                this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
            }
            break;
        case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
            if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING)
            {
                if (enqueue_tx_cmd(get_command_pause()) || this->optimistic_)
                {
                    this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
                }
            } 
            else 
            {
                if (enqueue_tx_cmd(get_command_play()) || this->optimistic_)
                {
                    this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
                }
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
    }

    this->publish_state();
}
}  // namespace uartex
}  // namespace esphome
