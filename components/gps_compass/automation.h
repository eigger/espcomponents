#pragma once

#include "esphome/core/automation.h"
#include "gps_compass.h"

namespace esphome {
namespace gps_compass {

// on_calibrated: a fit was accepted.  `first` is true for the first one since boot / level change / reset.
class CalibratedTrigger : public Trigger<bool, uint32_t> {
 public:
  explicit CalibratedTrigger(GpsCompass *parent) {
    parent->add_on_calibrated_callback([this](bool first, uint32_t samples) { this->trigger(first, samples); });
  }
};

// gps_compass.set_level: no option = the current pose is level; pitch / roll = raw angle (degrees) counted as level.
template<typename... Ts> class SetLevelAction : public Action<Ts...>, public Parented<GpsCompass> {
 public:
  TEMPLATABLE_VALUE(float, pitch)
  TEMPLATABLE_VALUE(float, roll)
  void play(const Ts &...x) override {
    if (!this->pitch_.has_value() && !this->roll_.has_value()) {
      this->parent_->level_to_current();
      return;
    }
    this->parent_->set_level_offsets(this->pitch_.optional_value(x...), this->roll_.optional_value(x...));
  }
};

template<typename... Ts> class ClearLevelAction : public Action<Ts...>, public Parented<GpsCompass> {
 public:
  void play(const Ts &...x) override { this->parent_->clear_level(); }
};

template<typename... Ts> class ResetCalibrationAction : public Action<Ts...>, public Parented<GpsCompass> {
 public:
  void play(const Ts &...x) override { this->parent_->reset_calibration(); }
};

}  // namespace gps_compass
}  // namespace esphome
