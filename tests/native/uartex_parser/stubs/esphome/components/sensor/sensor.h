#pragma once
#include "esphome/core/stub.h"
namespace esphome {
namespace sensor {
struct Sensor {
  float state = NAN;
  void add_on_state_callback(std::function<void(float)>) {}
};
}  // namespace sensor
}  // namespace esphome
