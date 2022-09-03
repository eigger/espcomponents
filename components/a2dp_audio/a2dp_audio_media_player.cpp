#include "a2dp_audio_media_player.h"
#include "a2dp_source.h"
#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/core/log.h"

namespace esphome {
namespace a2dp_audio {

static const char *const TAG = "audio";

char BT_SINK_NAME[]   = "Manhattan-165327"; // set your sink devicename here
char BT_SINK_PIN[]    = "1234";             // sink pincode
char BT_DEVICE_NAME[] = "ESP_A2DP_SRC";     // source devicename

void A2DPAudioMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (call.get_media_url().has_value()) {
    if (this->audio_->isRunning())
      this->audio_->stopSong();
    this->high_freq_.start();
    this->audio_->connecttohost(call.get_media_url().value().c_str());
    this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
  }
  if (call.get_volume().has_value()) {
    this->volume = call.get_volume().value();
    this->set_volume_(volume);
    this->unmute_();
  }
  if (call.get_command().has_value()) {
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_PLAY:
        if (!this->audio_->isRunning())
          this->audio_->pauseResume();
        this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
        if (this->audio_->isRunning())
          this->audio_->pauseResume();
        this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        this->stop_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->mute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->unmute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
        this->audio_->pauseResume();
        if (this->audio_->isRunning()) {
          this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        } else {
          this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
        }
        break;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP: {
        float new_volume = this->volume + 0.1f;
        if (new_volume > 1.0f)
          new_volume = 1.0f;
        this->set_volume_(new_volume);
        this->unmute_();
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN: {
        float new_volume = this->volume - 0.1f;
        if (new_volume < 0.0f)
          new_volume = 0.0f;
        this->set_volume_(new_volume);
        this->unmute_();
        break;
      }
    }
  }
  this->publish_state();
}

void A2DPAudioMediaPlayer::mute_() {
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->digital_write(true);
  } else {
    this->set_volume_(0.0f, false);
  }
  this->muted_ = true;
}
void A2DPAudioMediaPlayer::unmute_() {
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->digital_write(false);
  } else {
    this->set_volume_(this->volume, false);
  }
  this->muted_ = false;
}
void A2DPAudioMediaPlayer::set_volume_(float volume, bool publish) {
  this->audio_->setVolume(remap<uint8_t, float>(volume, 0.0f, 1.0f, 0, 21));
  if (publish)
    this->volume = volume;
}

void A2DPAudioMediaPlayer::stop_() {
  if (this->audio_->isRunning())
    this->audio_->stopSong();
  this->high_freq_.stop();
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

void A2DPAudioMediaPlayer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Audio...");
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    this->audio_ = make_unique<Audio>(true, this->internal_dac_mode_);
  } else {
    this->audio_ = make_unique<Audio>(false);
    this->audio_->setPinout(this->bclk_pin_, this->lrclk_pin_, this->dout_pin_);
    this->audio_->forceMono(this->external_dac_channels_ == 1);
    if (this->mute_pin_ != nullptr) {
      this->mute_pin_->setup();
      this->mute_pin_->digital_write(false);
    }
  }
  a2dp_source_init(BT_SINK_NAME, BT_SINK_PIN);
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

void A2DPAudioMediaPlayer::loop() {
  this->audio_->loop();
  if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING && !this->audio_->isRunning()) {
    this->stop_();
    this->publish_state();
  }
}

int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len) // BT data event
{
    static uint8_t bnr = 1;
    if (len < 0 || data == NULL) {
        return 0;
    }
    return len;
}

media_player::MediaPlayerTraits A2DPAudioMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause(true);
  return traits;
};

void A2DPAudioMediaPlayer::dump_config() {
  ESP_LOGCONFIG(TAG, "Audio:");
  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "Audio failed to initialize!");
    return;
  }
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    switch (this->internal_dac_mode_) {
      case I2S_DAC_CHANNEL_LEFT_EN:
        ESP_LOGCONFIG(TAG, "  Internal DAC mode: Left");
        break;
      case I2S_DAC_CHANNEL_RIGHT_EN:
        ESP_LOGCONFIG(TAG, "  Internal DAC mode: Right");
        break;
      case I2S_DAC_CHANNEL_BOTH_EN:
        ESP_LOGCONFIG(TAG, "  Internal DAC mode: Left & Right");
        break;
      default:
        break;
    }
  }
}

}  // namespace a2dp_audio
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
