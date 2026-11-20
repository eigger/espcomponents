#pragma once

#include "esphome/core/automation.h"
#include "ui_sound.h"

namespace esphome {
namespace ui_sound {

// ui_sound.play: `- ui_sound.play: click` or `{cue: click, force: !lambda ...}`.
template<typename... Ts> class PlayAction : public Action<Ts...>, public Parented<UiSound> {
 public:
  void set_cue(Cue *cue) { this->cue_ = cue; }
  TEMPLATABLE_VALUE(bool, force)
  void play(const Ts &...x) override { this->parent_->play(this->cue_, this->force_.value(x...)); }

 protected:
  Cue *cue_{nullptr};
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<UiSound> {
 public:
  void play(const Ts &...x) override { this->parent_->stop(); }
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<UiSound> {
 public:
  void play(const Ts &...x) override { this->parent_->start(); }
};

template<typename... Ts> class SetEnabledAction : public Action<Ts...>, public Parented<UiSound> {
 public:
  TEMPLATABLE_VALUE(bool, value)
  void play(const Ts &...x) override { this->parent_->set_enabled(this->value_.value(x...)); }
};

template<typename... Ts> class IsPlayingCondition : public Condition<Ts...>, public Parented<UiSound> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_playing(); }
};

}  // namespace ui_sound
}  // namespace esphome
