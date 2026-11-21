#include "uartex.h"
#include <cassert>
#include <vector>
using namespace esphome;
using namespace esphome::uartex;

class TestComponent : public UARTExComponent {
 public:
  void receive(uint8_t b) {
    bytes.push_back(b);
    assert(parse_bytes());
    publish_to_devices();
    rx_parser_.clear();
  }
  void poll() { write_to_uart(); }
  bool feed_raw() { return parse_bytes(); }
  void set_last_tx(unsigned long t) { tx_time_ = t; }
};

int main() {
  fake_millis = 100;
  TestComponent delayed;
  delayed.set_rx_length(1); delayed.set_tx_delay(50);
  delayed.add_rx_reply(state_t({1}), cmd_t({0xA1})); delayed.setup();
  std::vector<std::vector<uint8_t>> sent;
  delayed.add_on_write_callback([&](const uint8_t *p, uint16_t n) {sent.emplace_back(p, p+n);});
  delayed.write_command(cmd_t({0xB1}));
  delayed.receive(1);
  assert(sent.empty());
  fake_millis = 130; delayed.receive(1); // another frame restarts the common RX gap
  fake_millis = 179; delayed.poll(); assert(sent.empty());
  fake_millis = 180; delayed.poll(); assert(sent.size() == 1 && sent[0] == std::vector<uint8_t>{0xA1});
  fake_millis = 229; delayed.poll(); assert(sent.size() == 1);
  fake_millis = 230; delayed.poll(); assert(sent.size() == 2 && sent[1] == std::vector<uint8_t>{0xA1});
  fake_millis = 280; delayed.poll(); assert(sent.size() == 3 && sent[2] == std::vector<uint8_t>{0xB1});

  // Matching frames arrive faster than the quiet period. Only the oldest ten
  // replies may remain queued; later traffic must not grow the queue.
  fake_millis = 1000;
  TestComponent overloaded;
  overloaded.set_rx_length(2); overloaded.set_tx_delay(50);
  overloaded.add_rx_reply(state_t({1}), std::function<cmd_t(const uint8_t *, uint16_t)>(
      [](const uint8_t *data, uint16_t) { return cmd_t(std::vector<uint8_t>{data[1]}); }));
  overloaded.setup();
  std::vector<std::vector<uint8_t>> drained;
  overloaded.add_on_write_callback([&](const uint8_t *p, uint16_t n) {drained.emplace_back(p, p+n);});
  for (uint8_t i = 0; i < 30; ++i) {
    overloaded.bytes.push_back(1); assert(!overloaded.feed_raw());
    overloaded.receive(i);
    ++fake_millis;
  }
  assert(drained.empty());
  for (int i = 0; i < 12; ++i) { fake_millis += 50; overloaded.poll(); }
  assert(drained.size() == 10);
  for (uint8_t i = 0; i < 10; ++i) assert(drained[i] == std::vector<uint8_t>{i});

  fake_millis = 300;
  TestComponent tx_gap;
  tx_gap.set_rx_length(1); tx_gap.set_tx_delay(50);
  tx_gap.add_rx_reply(state_t({1}), cmd_t({0xA2})); tx_gap.setup();
  int tx_count = 0;
  tx_gap.add_on_write_callback([&](const uint8_t *, uint16_t) {++tx_count;});
  tx_gap.receive(1); tx_gap.set_last_tx(340);
  fake_millis = 389; tx_gap.poll(); assert(tx_count == 0);
  fake_millis = 390; tx_gap.poll(); assert(tx_count == 1);

  fake_millis = 400;
  TestComponent immediate;
  immediate.set_rx_length(1); immediate.set_tx_delay(0);
  immediate.add_rx_reply(state_t({1}), cmd_t({0xA3})); immediate.setup();
  int immediate_count = 0;
  immediate.add_on_write_callback([&](const uint8_t *, uint16_t) {++immediate_count;});
  immediate.receive(1); assert(immediate_count == 1);

  fake_millis = 500;
  TestComponent pending;
  pending.set_rx_length(1); pending.set_tx_delay(0); pending.set_tx_timeout(100);
  pending.add_rx_reply(state_t({1}), cmd_t({0xA4})); pending.setup();
  std::vector<std::vector<uint8_t>> pending_sent;
  pending.add_on_write_callback([&](const uint8_t *p, uint16_t n) {pending_sent.emplace_back(p, p+n);});
  pending.write_command(cmd_t({0xB2}, {2})); pending.poll();
  pending.write_command(cmd_t({0xB3}));
  pending.receive(1); assert(pending_sent.size() == 1);
  pending.receive(2); // ACK clears the pending command; reply precedes ordinary command
  assert(pending_sent.size() == 2 && pending_sent[1] == std::vector<uint8_t>{0xA4});
  pending.poll(); assert(pending_sent.size() == 3 && pending_sent[2] == std::vector<uint8_t>{0xB3});
}
