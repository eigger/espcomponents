#pragma once
#include "esphome/core/stub.h"

namespace esphome::switch_ {
class Switch : public EntityBase {
 public:
  bool state{false};
  bool is_inverted() const { return inverted_; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void control(bool target) { write_state(target != inverted_); }
  void publish_state(bool physical) { state = physical != inverted_; }

 protected:
  virtual void write_state(bool state) = 0;
  bool inverted_{false};
};
}  // namespace esphome::switch_
