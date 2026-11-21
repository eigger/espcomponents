#include "uartex.h"
#include <cassert>
using namespace esphome;
using namespace esphome::uartex;

class Bus : public UARTExComponent {
 public:
  bool select() { return select_ready_tx(tx_queue_); }
  const cmd_t &selected() { return *current_tx_data_.cmd; }
  void poll() { write_to_uart(); }
};

int main() {
  Bus order;
  UARTExDevice a, b;
  a.set_state("state", state_t({0x10}));
  b.set_state("state", state_t({0x20}));
  order.register_device(&a); order.register_device(&b);
  auto a1 = std::make_shared<const cmd_t>(cmd_t{{0x30, 0, 0}, {}, {}, {1, 2}});
  auto b1 = std::make_shared<const cmd_t>(cmd_t({0x55}));
  auto a2 = std::make_shared<const cmd_t>(cmd_t({0x66}));
  order.enqueue_tx_data({&a, a1.get(), a1});
  order.enqueue_tx_data({&b, b1.get(), b1});
  order.enqueue_tx_data({&a, a2.get(), a2});
  assert(order.select());
  assert((order.selected().data == std::vector<uint8_t>{0x55}));
  assert(!order.select()); // A2 cannot pass blocked A1
  a.parse_data({0x10, 1});
  assert(!order.select()); // state too short
  a.parse_data({0x10, 1, 2});
  assert(order.select());
  assert((order.selected().data == std::vector<uint8_t>{0x30, 1, 2}));
  assert(order.select());
  assert((order.selected().data == std::vector<uint8_t>{0x66}));

  Bus bus;
  UARTExDevice first, peer, unrelated;
  first.set_state("state", state_t({0x10, 0xA0}, {0xFF, 0xF0}));
  peer.set_state("state", state_t({0x10, 0xA0}, {0xFF, 0xF0}));
  unrelated.set_state("state", state_t({0x10, 0xB0}, {0xFF, 0xF0}));
  bus.register_device(&first); bus.register_device(&peer); bus.register_device(&unrelated);
  bus.set_tx_delay(0); bus.set_tx_timeout(10); bus.set_tx_retry_cnt(1);
  std::vector<std::vector<uint8_t>> sent;
  bus.add_on_write_callback([&](const uint8_t *p, uint16_t n) { sent.emplace_back(p, p+n); });
  const std::vector<uint8_t> initial{0x10, 0xA1, 0x22, 0x33, 0x44};
  first.parse_data(initial); peer.parse_data(initial);
  unrelated.parse_data({0x10, 0xB1, 0x44, 0x55, 0x66});
  auto cmd = std::make_shared<const cmd_t>(cmd_t{{0x30, 0xF1, 0, 0xF3, 0}, {0x40}, {}, {2, 4}});
  bus.enqueue_tx_data({&first, cmd.get(), cmd});
  bus.enqueue_tx_data({&peer, cmd.get(), cmd});
  fake_millis = 100;
  bus.poll();
  assert((sent == std::vector<std::vector<uint8_t>>{{0x30, 0xF1, 0x22, 0xF3, 0x44}}));
  // An RX after the first attempt rearms both, but retry must invalidate them again.
  first.parse_data({0x10, 0xA2, 0x66, 0x77, 0x88});
  peer.parse_data({0x10, 0xA2, 0x66, 0x77, 0x88});
  fake_millis = 110; bus.poll();
  assert(sent.size() == 2 && sent[0] == sent[1]);
  fake_millis = 120; bus.poll(); // timeout; peer remains deferred
  assert(sent.size() == 2);
  cmd_t rendered;
  assert(!peer.render_command(*cmd, rendered));
  assert(unrelated.render_command(*cmd, rendered));
  first.parse_data({0x10, 0xA3, 0x88, 0x99, 0xAA});
  peer.parse_data({0x10, 0xA3, 0x88, 0x99, 0xAA});
  bus.poll();
  assert((sent.back() == std::vector<uint8_t>{0x30, 0xF1, 0x88, 0xF3, 0xAA}));
  assert((cmd->data == std::vector<uint8_t>{0x30, 0xF1, 0, 0xF3, 0}));
}
