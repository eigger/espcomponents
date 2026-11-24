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

  // Bypass for the confirmation right after the entity's own command.
  struct TxBus : UARTExComponent { void poll() { write_to_uart(); } };
  {
    TxBus tx; Dev e, other;
    e.set_state("state", state_t({0x10})); other.set_state("state", state_t({0x10}));
    e.set_state_debounce(1000); other.set_state_debounce(1000);
    tx.register_device(&e); tx.register_device(&other);
    tx.set_tx_delay(0); tx.set_tx_timeout(10); tx.set_tx_retry_cnt(0);
    const cmd_t on({0x30, 0x01}), poll_cmd({0x31});
    fake_millis = 100; e.parse_data(off); other.parse_data(off);
    // Polling (low priority) does not arm the bypass.
    e.enqueue_tx_cmd(&poll_cmd, true);
    fake_millis = 200; tx.poll();
    fake_millis = 300; e.parse_data(heat);
    assert(e.published.size() == 1);
    fake_millis = 1300; e.flush_pending_state(1300);
    assert(e.published.size() == 2 && e.last_state() == heat);
    // Own command arms it; a stale equal packet does not consume it.
    e.enqueue_tx_cmd(&on);
    fake_millis = 2000; tx.poll();
    fake_millis = 2100; e.parse_data(heat);               // stale, equal to last: re-publish only
    assert(e.published.size() == 3);
    fake_millis = 2200; e.parse_data(cool);               // confirmation: immediate
    assert(e.published.size() == 4 && e.last_state() == cool);
    // One-shot: the next differing packet is debounced again.
    fake_millis = 2300; e.parse_data(off);
    assert(e.published.size() == 4);
    fake_millis = 3300; e.flush_pending_state(3300);
    assert(e.published.size() == 5 && e.last_state() == off);
    // A device that did not send anything is not affected by the other's command.
    fake_millis = 3400; other.parse_data(heat);
    assert(other.published.size() == 1);
    // Window expiry: back to normal debounce.
    e.enqueue_tx_cmd(&on);
    fake_millis = 4000; tx.poll();
    fake_millis = 5001; e.parse_data(cool);
    assert(e.published.size() == 5);
    fake_millis = 6001; e.flush_pending_state(6001);
    assert(e.published.size() == 6 && e.last_state() == cool);
  }

  // Retry re-arms the window; write_command / rx replies never arm it.
  {
    TxBus tx; Dev e;
    e.set_state("state", state_t({0x10}));
    e.set_state_debounce(1000);
    tx.register_device(&e);
    tx.set_tx_delay(0); tx.set_tx_timeout(600); tx.set_tx_retry_cnt(1);
    const cmd_t on({0x30, 0x01}, {0x99});
    fake_millis = 100; e.parse_data(off);
    e.enqueue_tx_cmd(&on);
    fake_millis = 2000; tx.poll();           // first transmission arms until 3000
    fake_millis = 2600; tx.poll();           // retry re-arms until 3600
    fake_millis = 3300; e.parse_data(heat);  // after the first window, inside the retry window
    assert(e.published.size() == 2 && e.last_state() == heat);

    tx.write_command(cmd_t({0x40}));
    auto reply = std::make_shared<const cmd_t>(cmd_t({0x41}));
    assert(tx.enqueue_tx_reply({&e, reply.get(), reply}));
    fake_millis = 10000; tx.poll();
    fake_millis = 10100; tx.poll();
    fake_millis = 10200; e.parse_data(cool);
    assert(e.published.size() == 2);         // still debounced: nothing armed
    fake_millis = 11200; e.flush_pending_state(11200);
    assert(e.published.size() == 3 && e.last_state() == cool);
  }

  // Masked comparison: only byte 1 (mode) counts, byte 2 (current temperature) is free.
  {
    struct TxBus2 : UARTExComponent { void poll() { write_to_uart(); } };
    TxBus2 tx; Dev m;
    m.set_state("state", state_t({0x10}));
    m.set_state_debounce(1000);
    m.set_state_debounce_mask(1, {0xFF});
    tx.register_device(&m);
    tx.set_tx_delay(0); tx.set_tx_timeout(10); tx.set_tx_retry_cnt(0);
    fake_millis = 100; m.parse_data({0x10, 0x00, 20});
    assert(m.published.size() == 1);
    fake_millis = 200; m.parse_data({0x10, 0x00, 21});          // masked-equal: immediate
    assert(m.published.size() == 2 && m.last_state()[2] == 21);
    fake_millis = 300; m.parse_data({0x10, 0x01, 21});          // masked-different: pending
    assert(m.published.size() == 2);
    fake_millis = 400; m.parse_data({0x10, 0x01, 22});          // latest wins, timer not restarted
    fake_millis = 1300; m.flush_pending_state(1299);
    assert(m.published.size() == 2);
    m.flush_pending_state(1300);
    assert(m.published.size() == 3 && m.last_state() == (std::vector<uint8_t>{0x10, 0x01, 22}));
    // Glitch revert: masked-equal packet cancels pending and applies at once.
    fake_millis = 2000; m.parse_data({0x10, 0x00, 22});
    assert(m.published.size() == 3);                            // pending glitch
    fake_millis = 2100; m.parse_data({0x10, 0x01, 23});         // reverted: applied at once
    assert(m.published.size() == 4 && m.last_state()[2] == 23);
    m.flush_pending_state(9000);
    assert(m.published.size() == 4);
    // Bypass with a mask: confirmation right after our command is immediate.
    const cmd_t on({0x30, 0x01});
    m.enqueue_tx_cmd(&on);
    fake_millis = 3000; tx.poll();
    fake_millis = 3100; m.parse_data({0x10, 0x01, 24});         // masked-equal: normal immediate, bypass kept
    assert(m.published.size() == 5);
    fake_millis = 3200; m.parse_data({0x10, 0x02, 24});
    assert(m.published.size() == 6 && m.last_state()[1] == 0x02);
    fake_millis = 3300; m.parse_data({0x10, 0x03, 24});         // one-shot: debounced now
    assert(m.published.size() == 6);
    // Out of range: a masked byte present in one packet only is a change.
    fake_millis = 5000; m.flush_pending_state(5000);
    assert(m.published.size() == 7 && m.last_state()[1] == 0x03);
    fake_millis = 5100; m.parse_data({0x10});
    assert(m.published.size() == 7);                            // different -> pending
    m.flush_pending_state(6100);
    assert(m.published.size() == 8 && m.last_state().size() == 1);
    fake_millis = 6200; m.parse_data({0x10, 0x03, 24});
    assert(m.published.size() == 8);
    m.flush_pending_state(7200);
    assert(m.published.size() == 9 && m.last_state().size() == 3);
    // Both packets lack the masked byte: equal, applied immediately.
    Dev n; n.set_state("state", state_t({0x10})); n.set_state_debounce(1000); n.set_state_debounce_mask(5, {0xFF});
    n.parse_data({0x10, 1}); n.parse_data({0x10, 2});
    assert(n.published.size() == 2);
  }

  // Mask covering the state_on/state_off byte: bool publish follows the mask decision.
  {
    Dev b;
    b.set_state("state", state_t({0x10}));
    b.set_state("state_on", state_t({0x01}, {}, 1));
    b.set_state("state_off", state_t({0x00}, {}, 1));
    b.set_state_debounce(1000);
    b.set_state_debounce_mask(1, {0xFF});
    fake_millis = 100; b.parse_data({0x10, 0x00, 20});
    assert((b.bools == std::vector<bool>{false}));
    fake_millis = 200; b.parse_data({0x10, 0x00, 21});          // masked-equal: applied, off again
    assert((b.bools == std::vector<bool>{false, false}));
    fake_millis = 300; b.parse_data({0x10, 0x01, 21});          // masked-different: held, no bool
    assert(b.bools.size() == 2);
    b.flush_pending_state(1300);
    assert((b.bools == std::vector<bool>{false, false, true}));
    fake_millis = 1400; b.parse_data({0x10, 0x00, 21});         // glitch back is also held
    assert(b.bools.size() == 3);
    fake_millis = 1500; b.parse_data({0x10, 0x01, 22});         // revert: applied at once as on
    assert((b.bools == std::vector<bool>{false, false, true, true}));
  }

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
