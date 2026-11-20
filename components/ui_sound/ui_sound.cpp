#include "ui_sound.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ui_sound {

static const char *const TAG = "ui_sound";

void UiSound::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UI sound:\n"
                "  Announcement: %s\n"
                "  Min gap: %" PRIu32 " ms\n"
                "  Start grace: %" PRIu32 " ms\n"
                "  Max busy: %" PRIu32 " ms\n"
                "  Require start: %s\n"
                "  Skip when muted: %s",
                YESNO(this->announcement_), this->min_gap_ms_, this->start_grace_ms_, this->max_busy_ms_,
                YESNO(this->require_start_), YESNO(this->skip_when_muted_));
}

bool UiSound::busy_(uint32_t now) {
  if (!this->issued_)
    return false;
  const uint32_t elapsed = now - this->issued_ms_;
  if (elapsed >= this->max_busy_ms_) {  // cap: a stuck state must not block cues forever
    this->issued_ = false;
    return false;
  }
  if (elapsed < this->start_grace_ms_)  // the state has not caught up with the play request yet
    return true;
  if (elapsed < this->issued_duration_ms_)  // the cue's declared length (files with trailing silence)
    return true;
  const auto state = this->player_->state;
  if (state == media_player::MEDIA_PLAYER_STATE_PLAYING || state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING)
    return true;
  this->issued_ = false;
  return false;
}

PlayResult UiSound::play(Cue *cue, bool force) {
  if (cue == nullptr || cue->file == nullptr || this->player_ == nullptr)
    return PlayResult::INVALID;
  if (!this->enabled_)
    return PlayResult::DISABLED;
  // Opt-in: the player applies mute / volume commands from a queue, so this state lags behind the automation.
  if (this->skip_when_muted_ && (this->player_->is_muted() || this->player_->volume <= 0.0f))
    return PlayResult::MUTED;
  // play_file() silently ignores commands before the player is set up: report it, keep all state untouched.
  if (!this->player_->is_ready())
    return PlayResult::NOT_READY;
  const uint32_t now = millis();
  bool interrupt = false;
  if (!force) {
    if (this->require_start_ && !this->started_)
      return PlayResult::NOT_STARTED;
    if (cue->played && now - cue->last_start_ms < cue->cooldown_ms)
      return PlayResult::COOLDOWN;
    if (this->any_played_ && now - this->issued_ms_ < this->min_gap_ms_)
      return PlayResult::MIN_GAP;
    if (this->busy_(now)) {
      if (cue->priority <= this->issued_priority_)
        return PlayResult::BUSY;
      interrupt = true;
    }
  } else {
    interrupt = this->busy_(now);
  }
  // enqueue=false: the new cue replaces whatever is playing; nothing is ever queued.
  this->player_->play_file(cue->file, this->announcement_, false);
  cue->played = true;
  cue->last_start_ms = now;
  this->any_played_ = true;
  this->issued_ = true;
  this->issued_ms_ = now;
  this->issued_duration_ms_ = cue->duration_ms;
  this->issued_priority_ = cue->priority;
  return interrupt ? PlayResult::INTERRUPTED : PlayResult::STARTED;
}

void UiSound::stop() {
  if (this->player_ != nullptr && this->issued_) {
    this->player_->make_call().set_command(media_player::MEDIA_PLAYER_COMMAND_STOP).set_announcement(this->announcement_)
        .perform();
  }
  this->issued_ = false;
}

}  // namespace ui_sound
}  // namespace esphome
