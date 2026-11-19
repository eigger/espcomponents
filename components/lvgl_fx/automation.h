#pragma once

#include "esphome/core/automation.h"
#include "lvgl_fx.h"

namespace esphome {
namespace lvgl_fx {

// lvgl_fx.start: declare the UI ready (policy row 2).
template<typename... Ts> class StartAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override { start(); }
};

// lvgl_fx.set_enabled / lvgl_fx.set_reduced_motion: the runtime policy switches (policy row 1).
template<typename... Ts> class SetEnabledAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(bool, value)
  void play(const Ts &...x) override { lvgl_fx::set_enabled(this->value_.value(x...)); }
};

template<typename... Ts> class SetReducedMotionAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(bool, value)
  void play(const Ts &...x) override { lvgl_fx::set_reduced_motion(this->value_.value(x...)); }
};

}  // namespace lvgl_fx
}  // namespace esphome
