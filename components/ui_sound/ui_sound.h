#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/audio/audio.h"
#include "esphome/components/speaker/media_player/speaker_media_player.h"

namespace esphome {
namespace ui_sound {

/// One cue: the media file and its policy values.  Created from the YAML `cues:` list.
struct Cue {
  Cue(const audio::AudioFile *file, uint8_t priority, uint32_t cooldown_ms, uint32_t duration_ms)
      : file(file), priority(priority), cooldown_ms(cooldown_ms), duration_ms(duration_ms) {}

  const audio::AudioFile *file;
  uint8_t priority;
  uint32_t cooldown_ms;  // minimum time between two starts of this cue
  uint32_t duration_ms;  // optional busy window since the start (0: none)
  uint32_t last_start_ms{0};
  bool played{false};  // last_start_ms is valid
};

/// What `UiSound::play()` did.  Only STARTED and INTERRUPTED mean the cue was handed to the player.
enum class PlayResult : uint8_t {
  STARTED,      // played, nothing was busy
  INTERRUPTED,  // played, cut a lower-priority cue
  BUSY,         // dropped: a cue of equal or higher priority is playing
  COOLDOWN,     // dropped: the cue's own cooldown is not over
  MIN_GAP,      // dropped: the global min_gap is not over
  MUTED,        // dropped: skip_when_muted is set and the player is muted or at volume 0
  DISABLED,     // dropped: set_enabled(false)
  NOT_STARTED,  // dropped: require_start is set and start() has not been called
  NOT_READY,    // dropped: the media player is not set up yet (nothing recorded)
  INVALID,      // dropped: null cue / no player
};

inline bool played(PlayResult r) { return r == PlayResult::STARTED || r == PlayResult::INTERRUPTED; }

class UiSound : public Component {
 public:
  void set_media_player(speaker::SpeakerMediaPlayer *player) { this->player_ = player; }
  void set_announcement(bool announcement) { this->announcement_ = announcement; }
  void set_min_gap(uint32_t ms) { this->min_gap_ms_ = ms; }
  void set_start_grace(uint32_t ms) { this->start_grace_ms_ = ms; }
  void set_max_busy(uint32_t ms) { this->max_busy_ms_ = ms; }
  void set_require_start(bool require) { this->require_start_ = require; }
  void set_skip_when_muted(bool skip) { this->skip_when_muted_ = skip; }

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  /// Play `cue` if the policy allows it (see README "Policy").  `force` skips start / cooldown / min_gap / priority.
  PlayResult play(Cue *cue, bool force = false);
  /// Stop the cue that is playing and clear the busy state.
  void stop();
  /// Declare the UI ready (only matters with `require_start`).
  void start() { this->started_ = true; }
  void set_enabled(bool enabled) { this->enabled_ = enabled; }
  bool is_enabled() const { return this->enabled_; }
  /// True while one of our cues is considered playing (state, start grace, cue duration, capped by max_busy).
  bool is_playing() { return this->busy_(millis()); }

 protected:
  bool busy_(uint32_t now);

  speaker::SpeakerMediaPlayer *player_{nullptr};
  uint32_t min_gap_ms_{0};
  uint32_t start_grace_ms_{300};
  uint32_t max_busy_ms_{10000};
  bool announcement_{true};
  bool require_start_{false};
  bool skip_when_muted_{false};
  bool started_{false};
  bool enabled_{true};

  // Last issued cue: what the busy window and the priority are measured against.
  bool issued_{false};
  uint32_t issued_ms_{0};
  uint32_t issued_duration_ms_{0};
  uint8_t issued_priority_{0};
  bool any_played_{false};
};

}  // namespace ui_sound
}  // namespace esphome
