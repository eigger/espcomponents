#include "automation.h"
#include <cassert>
#include <functional>
#include <vector>
using namespace esphome::uartex;

class TestComponent : public UARTExComponent {
 public:
  void read(const std::vector<uint8_t> &bytes) { read_callback_.call(bytes.data(), bytes.size()); }
  void write(const std::vector<uint8_t> &bytes) { write_callback_.call(bytes.data(), bytes.size()); }
};

int main() {
  TestComponent component;
  ReadTrigger read_trigger(&component);
  WriteTrigger write_trigger(&component);
  std::vector<std::function<void()>> continuations;
  auto capture = [&continuations](const uint8_t *data, uint16_t len, automation_data_owner_t owner) {
    // Like DelayAction, capture every trigger argument by value.
    const uint8_t expected = data[0];
    continuations.emplace_back([data, len, owner, expected]() {
      assert(owner->data() == data && owner->size() == len);
      assert(len == 2 && data[0] == expected && data[1] == static_cast<uint8_t>(expected + 1));
    });
  };
  read_trigger.handler = capture;
  write_trigger.handler = capture;
  for (uint8_t n : {1, 3, 5, 7}) {
    std::vector<uint8_t> source{n, static_cast<uint8_t>(n + 1)};
    if (n < 5) component.read(source); else component.write(source);
    source.assign(100, 0); // invalidate and overwrite the source before continuation
  }
  assert(continuations.size() == 4);
  for (auto &continuation : continuations) continuation();
}
