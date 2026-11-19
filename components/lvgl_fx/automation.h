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

}  // namespace lvgl_fx
}  // namespace esphome
