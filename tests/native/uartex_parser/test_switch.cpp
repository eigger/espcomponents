#include "switch/uartex_switch.h"
#include "uartex.h"
#include <cassert>
using namespace esphome::uartex;

int main() {
  UARTExSwitch device;
  device.set_optimistic(false);  // wait for state feedback
  device.set_command("command_on", cmd_t({1}, {0xA1}));
  device.set_command("command_off", cmd_t({0}, {0xA0}));
  device.set_state("state_on", state_t({0x11}));
  device.set_state("state_off", state_t({0x10}));

  UARTExComponent bus;
  bus.set_tx_delay(0);
  bus.set_tx_timeout(10);
  bus.set_tx_retry_cnt(1);
  bus.register_device(&device);
  bus.setup();
  std::vector<uint8_t> writes;
  bus.add_on_write_callback([&](const uint8_t *data, uint16_t) { writes.push_back(data[0]); });

  device.parse_data({0x10});
  device.control(true);
  bus.loop();
  esphome::fake_millis += 10;
  bus.loop();
  esphome::fake_millis += 10;
  bus.loop();
  assert(!device.state && (writes == std::vector<uint8_t>{1, 1}));
  // A timed-out ON can be requested again, then countermanded before feedback.
  device.control(true);
  device.control(false);
  bus.loop();
  assert(!device.state && (writes == std::vector<uint8_t>{1, 1, 1}));
  assert(device.dequeue_tx_cmd()->data[0] == 0);
  device.parse_data({0x11});
  assert(device.state);

  UARTExSwitch inverted;
  inverted.set_command("command_on", cmd_t({1}));
  inverted.set_command("command_off", cmd_t({0}));
  inverted.set_inverted(true);
  inverted.set_optimistic(true);
  inverted.control(false);
  assert(!inverted.state && inverted.dequeue_tx_cmd()->data[0] == 1);
  inverted.control(false);
  assert(!inverted.dequeue_tx_cmd());
  inverted.control(true);
  assert(inverted.state && inverted.dequeue_tx_cmd()->data[0] == 0);

  // optimistic defaults to true: the requested state is reflected at once, with or without a command.
  UARTExSwitch dflt;
  dflt.set_command("command_on", cmd_t({1}));
  dflt.control(true);
  assert(dflt.state && dflt.dequeue_tx_cmd()->data[0] == 1);
  UARTExSwitch no_cmd;
  no_cmd.control(true);
  assert(no_cmd.state && !no_cmd.dequeue_tx_cmd());

  // optimistic false: the command is queued but the state only follows feedback.
  UARTExSwitch waiting;
  waiting.set_optimistic(false);
  waiting.set_command("command_on", cmd_t({1}));
  waiting.set_state("state_on", state_t({0x11}));
  waiting.control(true);
  assert(!waiting.state && waiting.dequeue_tx_cmd()->data[0] == 1);
  waiting.parse_data({0x11});
  assert(waiting.state);

  // A configured command lambda that returns no data is a rejection: nothing is queued and the state is not reflected.
  UARTExSwitch rejected;
  rejected.set_command("command_on", [&]() -> cmd_t { return {}; });
  rejected.control(true);
  assert(!rejected.state && !rejected.dequeue_tx_cmd());
}
