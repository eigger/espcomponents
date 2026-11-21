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
  void replay(uint8_t byte) { rx_replay_.push_back(byte); }
  bool read_raw(uint8_t &byte) { return read_rx_byte(byte); }
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

  // Even an incomplete or rejected frame keeps the bus busy until the last
  // physical byte's quiet period has elapsed.
  fake_millis = 600;
  TestComponent malformed;
  malformed.set_rx_priority(PRIORITY_LOOP);
  malformed.set_rx_length(2); malformed.set_rx_timeout(10); malformed.set_tx_delay(50);
  malformed.add_rx_header({{0xAA}, {}}); malformed.setup();
  int malformed_sent = 0;
  malformed.add_on_write_callback([&](const uint8_t *, uint16_t) { ++malformed_sent; });
  malformed.write_command(cmd_t({0xCC}));
  fake_millis = 650; malformed.bytes.push_back(0xAA); malformed.loop();
  fake_millis = 670; malformed.loop(); // incomplete candidate times out
  assert(malformed_sent == 0);
  fake_millis = 699; malformed.loop(); assert(malformed_sent == 0);
  fake_millis = 700; malformed.loop(); assert(malformed_sent == 1);

  // A fully received but checksum-invalid frame also restarts the quiet
  // period, in both blocking-data and incremental-loop receive modes.
  for (auto priority : {PRIORITY_DATA, PRIORITY_LOOP}) {
    fake_millis = 2000;
    TestComponent bad_checksum;
    bad_checksum.set_rx_priority(priority);
    bad_checksum.set_rx_length([](const uint8_t *, uint16_t) -> int16_t { return 3; });
    bad_checksum.set_rx_checksum(CHECKSUM_XOR);
    bad_checksum.set_tx_delay(50); bad_checksum.add_rx_header({{0xAA}, {}});
    bad_checksum.setup();
    int writes = 0, checksum_errors = 0;
    bad_checksum.add_on_write_callback([&](const uint8_t *, uint16_t) { ++writes; });
    bad_checksum.add_on_error_callback([&](ERROR error) { if (error == ERROR_CHECKSUM) ++checksum_errors; });
    bad_checksum.write_command(cmd_t({0xCC}));
    fake_millis = 2050;
    for (uint8_t byte : {0xAA, 0x01, 0x00}) bad_checksum.bytes.push_back(byte);
    bad_checksum.loop();
    assert(checksum_errors == 1 && writes == 0);
    fake_millis = 2099; bad_checksum.loop(); assert(writes == 0);
    fake_millis = 2100; bad_checksum.loop(); assert(writes == 1);
  }

  // Reprocessing a buffered byte must not count as new physical bus traffic.
  fake_millis = 2200;
  TestComponent replayed;
  replayed.set_tx_delay(50); replayed.setup();
  int replay_writes = 0;
  replayed.add_on_write_callback([&](const uint8_t *, uint16_t) { ++replay_writes; });
  replayed.write_command(cmd_t({0xCC}));
  uint8_t raw;
  fake_millis = 2250; replayed.bytes.push_back(0xAA); assert(replayed.read_raw(raw));
  fake_millis = 2299; replayed.replay(raw); assert(replayed.read_raw(raw));
  fake_millis = 2300; replayed.poll(); assert(replay_writes == 1);

  fake_millis = 800;
  TestComponent retries;
  retries.set_tx_delay(0); retries.set_tx_timeout(10); retries.set_tx_retry_cnt(2); retries.setup();
  int attempts = 0, timeouts = 0;
  retries.add_on_write_callback([&](const uint8_t *, uint16_t) { ++attempts; });
  retries.add_on_error_callback([&](ERROR error) { if (error == ERROR_TX_TIMEOUT) ++timeouts; });
  retries.write_command(cmd_t({0xDD}, {0xEE})); retries.poll();
  assert(attempts == 1 && timeouts == 0);
  fake_millis = 810; retries.poll(); assert(attempts == 2 && timeouts == 0);
  fake_millis = 820; retries.poll(); assert(attempts == 3 && timeouts == 0);
  fake_millis = 830; retries.poll(); assert(attempts == 3 && timeouts == 1);

  fake_millis = 1000;
  TestComponent shared;
  shared.set_rx_priority(PRIORITY_LOOP);
  shared.set_tx_delay(5);
  shared.set_tx_timeout(50);
  shared.set_rx_checksum(CHECKSUM_XOR);
  shared.set_tx_checksum(CHECKSUM_XOR_NO_HEADER);
  shared.add_rx_header({{0xA0}, {}});
  shared.add_rx_header({{0xB0}, {}});
  shared.set_rx_length([](const uint8_t *data, uint16_t len) -> int16_t {
    if (len < 2) return 0;
    if (data[1] == 0xFF) return -1;
    return data[0] == 0xA0 ? 4 : 5;
  });
  shared.setup();

  std::vector<std::vector<uint8_t>> shared_rx;
  std::vector<std::vector<uint8_t>> shared_tx;
  shared.add_on_read_callback([&](const uint8_t *data, uint16_t len) {
    shared_rx.emplace_back(data, data + len);
    if (data[0] == 0xA0) shared.write_immediate_reply({0xC0, data[1]});
  });
  shared.add_on_write_callback([&](const uint8_t *data, uint16_t len) {
    shared_tx.emplace_back(data, data + len);
  });

  // A queued command can use the idle gap between polls, even when the polls
  // are less than 50 ms apart.
  for (uint8_t byte : {0xA0, 1, 2, 0xA3}) shared.bytes.push_back(byte);
  shared.loop();
  shared.write_command(cmd_t({0xD0}, {0x10}));
  fake_millis = 1004;
  shared.loop();
  assert(shared_tx.size() == 1);
  fake_millis = 1005;
  shared.loop();
  assert((shared_tx == std::vector<std::vector<uint8_t>>{{0xC0, 1, 0xC1}, {0xD0, 0xD0}}));
  fake_millis = 1010;
  // The extra A0 gives a bad checksum at the first proposed boundary. Its
  // suffix contains the next valid frame and must survive the rejection.
  for (uint8_t byte : {0xA0, 0xA0, 1, 2, 0xA3, 0xB0, 3, 4, 5, 0xB2})
    shared.bytes.push_back(byte);
  shared.loop();
  shared.loop();
  fake_millis = 1025;
  for (uint8_t byte : {0xA0, 0xFF, 0xA0, 1, 2, 0xA3}) shared.bytes.push_back(byte);
  shared.loop();
  assert(shared_rx.size() == 4 && shared_rx[1] == shared_rx[0] && shared_rx[2].size() == 5 && shared_rx[3] == shared_rx[0]);
  assert((shared_tx == std::vector<std::vector<uint8_t>>{{0xC0, 1, 0xC1}, {0xD0, 0xD0}, {0xC0, 1, 0xC1}, {0xC0, 1, 0xC1}}));

  fake_millis = 1054;
  shared.loop();
  assert(shared_tx.size() == 4);
  fake_millis = 1055;
  shared.loop();
  assert(shared_tx.size() == 5 && shared_tx.back() == shared_tx[1]);
}
