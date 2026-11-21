#pragma once
#include "stub.h"
namespace esphome {
template<typename... Ts> class Trigger {
 public:
  std::function<void(Ts...)> handler;
  void trigger(Ts... args) { if (handler) handler(args...); }
};
template<typename... Ts> class Action {
 public:
  virtual ~Action() = default;
  virtual void play(const Ts &...args) = 0;
};
template<typename T> class Parented { protected: T *parent_{}; };
}
