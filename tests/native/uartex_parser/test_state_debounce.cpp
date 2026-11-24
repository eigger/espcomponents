#include "uartex.h"
#include <cassert>
using namespace esphome;
using namespace esphome::uartex;

class Dev : public UARTExDevice {
 public:
  std::vector<std::vector<uint8_t>> published;
  std::vector<bool> bools;
  void publish(const std::vector<uint8_t> &d) override { published.push_back(d); }
  void publish(const bool s) override { bools.push_back(s); }
};

int main() {
  const std::vector<uint8_t> off{0x10, 0x00}, heat{0x10, 0x01}, cool{0x10, 0x02};

  // Disabled: identical to the previous behavior.
  Dev plain;
  plain.set_state("state", state_t({0x10}));
  plain.parse_data(off); plain.parse_data(heat);
  assert(plain.published.size() == 2 && plain.last_state() == heat);
  assert(plain.bools.empty());
  plain.set_state("state_on", state_t({0x01}, {}, 1));
  plain.set_state("state_off", state_t({0x00}, {}, 1));
  plain.parse_data(off); plain.parse_data(heat);
  assert((plain.bools == std::vector<bool>{false, true}));

  Dev d;
  d.set_state("state", state_t({0x10}));
  d.set_state("state_on", state_t({0x01}, {}, 1));
  d.set_state_debounce(2000);
  fake_millis = 1000;
  assert(d.parse_data(off));
  assert(d.published.size() == 1 && d.has_last_state());  // first packet is immediate
  assert(!d.parse_data({0x20, 0x00}));                    // non-matching packet ignored
  // Glitch off -> heat -> off is filtered.
  fake_millis = 2000; d.parse_data(heat);
  fake_millis = 3000; d.flush_pending_state(3000);
  assert(d.published.size() == 1);
  d.parse_data(off);                         // reverted: re-published, pending dropped
  assert(d.published.size() == 2);
  fake_millis = 9000; d.flush_pending_state(9000);
  assert(d.published.size() == 2 && d.last_state() == off);
  // Real change: timer starts at the first change, latest packet wins.
  fake_millis = 10000; d.parse_data(heat);
  fake_millis = 11000; d.parse_data(cool);
  d.flush_pending_state(11999);
  assert(d.published.size() == 2);
  d.flush_pending_state(12000);
  assert(d.published.size() == 3 && d.published.back() == cool && d.last_state() == cool);
  d.flush_pending_state(20000);
  assert(d.published.size() == 3);  // nothing pending anymore
  // bool publish happens on apply only.
  fake_millis = 21000; d.parse_data(heat);
  assert(d.bools.empty());
  d.flush_pending_state(23000);
  assert(d.bools.size() == 1 && d.bools[0]);
  // millis() wraparound.
  fake_millis = 0xFFFFFF00UL; d.parse_data(off);
  d.flush_pending_state(0xFFFFFFF0u);
  assert(d.last_state() == heat);
  d.flush_pending_state(0xFFFFFF00u + 2000u);  // wrapped
  assert(d.last_state() == off);

  // Entity state changed by control(): an equal-to-last packet is re-published at once.
  Dev c;
  c.set_state("state", state_t({0x10}));
  c.set_state_debounce(2000);
  fake_millis = 100; c.parse_data(off);
  fake_millis = 200; c.parse_data(heat);   // pending
  c.parse_data(off);                       // wallpad ignored the command: reverts
  assert(c.published.size() == 2 && c.published.back() == off);
  fake_millis = 5000; c.flush_pending_state(5000);
  assert(c.published.size() == 2);         // nothing left pending

  // Wired through UARTExComponent::loop(): fake millis + fed frames.
  struct Bus : UARTExComponent { bool feed(uint8_t b) { bytes.push_back(b); return true; } };
  Bus bus; Dev w;
  w.set_state("state", state_t({0x10}));
  w.set_state_debounce(1000);
  bus.register_device(&w);
  bus.set_rx_length(2); bus.set_rx_priority(PRIORITY_LOOP); bus.setup();
  fake_millis = 100; bus.feed(0x10); bus.feed(0x00); bus.loop();
  assert(w.published.size() == 1);
  fake_millis = 200; bus.feed(0x10); bus.feed(0x01); bus.loop();
  assert(w.published.size() == 1);
  fake_millis = 1199; bus.loop();
  assert(w.published.size() == 1);
  fake_millis = 1200; bus.loop();
  assert(w.published.size() == 2 && w.published.back() == heat);

  // preserve_state: waits while a change is pending, recovers after an equal packet.
  Dev p;
  p.set_state("state", state_t({0x10}));
  p.set_state_debounce(1000);
  const cmd_t cmd{{0x30, 0, 0}, {}, {}, {1}};
  fake_millis = 100; p.parse_data(off);
  cmd_t out;
  assert(p.render_command(cmd, out));
  fake_millis = 200; p.parse_data(heat);
  assert(!p.render_command(cmd, out));       // pending change: last_state_ is stale
  p.flush_pending_state(1200);
  assert(p.render_command(cmd, out) && out.data[1] == 0x01);
  p.invalidate_matching_state(heat);
  assert(!p.render_command(cmd, out));
  p.parse_data(heat);                         // equal to last_state_: becomes ready again
  assert(p.render_command(cmd, out));
  return 0;
}
