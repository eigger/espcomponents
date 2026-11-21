"""Exercise the C++ lambdas embedded in wallpad packet definitions."""
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[4]
ROOT = REPO / 'packages/wallpad'


def lambdas(path, key):
    text = (ROOT / path).read_text()
    return re.findall(r'^    ' + key + r': !lambda \|-\n((?:      .*\n)+)', text, re.M)


def compile_run(source):
    with tempfile.TemporaryDirectory() as tmp:
        src, exe = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
        src.write_text(source)
        subprocess.run(['c++', '-std=c++11', str(src), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


class FrameTests(unittest.TestCase):
    def test_samsung_checksums(self):
        def body(path):
            text = (ROOT / path).read_text()
            return re.search(r'^  rx_checksum: !lambda \|-\n((?:    .*\n)+)', text, re.M)[1]
        source = '''#include <cstdint>
#include <cstddef>
#include <cassert>
uint8_t standalone(const uint8_t *data, size_t len, const uint8_t *header) { BODY1 }
uint8_t shared(const uint8_t *data, size_t len, const uint8_t *header) { BODY2 }
int main() {
  uint8_t h[] = {0xb0};
  uint8_t payload[] = {0x79, 0x31, 0x80};
  for (int op = 0; op < 256; ++op) {
    payload[0] = op;
    assert(shared(payload, 3, h) == standalone(payload, 3, h));
  }
  h[0] = 0xa4; payload[0] = 0x31; payload[1] = 0;
  assert(shared(payload, 2, h) == 0x15);
}
'''.replace('BODY1', body('samsung/samsung_sds.yaml')).replace('BODY2', body('samsung/samsung_sds_shared.yaml'))
        compile_run(source)

    def test_heater_short_payloads(self):
        for path, base in [('ezville/ezville.yaml', 11), ('cvnet/cvnet.yaml', 6)]:
            text = (ROOT / path).read_text()
            section = text.split('climate:', 1)[1].split('\nvalve:', 1)[0].split('\nsensor:', 1)[0]
            states = re.findall(r'    state:\n      data: \[([^]]+)\]\n      mask: \[([^]]+)\]', section)
            self.assertEqual(len(states), 4)
            checks = ''
            for i, (data, mask) in enumerate(states):
                length = base + i * 2
                checks += f"""
  {{ state_t state({{{data}}}, {{{mask}}});
     std::vector<uint8_t> packet = {{{data}}};
     assert(packet.size() == {length});
     for (size_t n = 0; n < packet.size(); ++n) {{
       auto truncated = packet; truncated.resize(n);
       assert(!verify_state(truncated, &state));
     }}
     for (size_t n = 3; n < packet.size(); ++n) packet[n] = 0xff;
     assert(verify_state(packet, &state));
  }}
"""
            self.compile_component('#include "uartex_device.h"\n#include <cassert>\nusing namespace esphome::uartex;\nint main() {' + checks + '}')

    def compile_component(self, source):
        with tempfile.TemporaryDirectory() as tmp:
            src, exe = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
            src.write_text(source)
            component = REPO / 'components/uartex'
            subprocess.run(['c++', '-std=c++17', '-I' + str(REPO / 'tests/native/uartex_parser/stubs'),
                            '-I' + str(component), *(str(component / f) for f in
                            ('uartex.cpp', 'uartex_device.cpp', 'parser.cpp', 'checksum.cpp')),
                            str(src), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_hyundai_component_accepts_embedded_footer(self):
        text = (ROOT / 'hyundai/hyundai_imazu.yaml').read_text()
        self.assertRegex(text, r'rx_data_length:\n    offset: 0\n    length: 1\n    adjust: -4')
        source = """#include "uartex.h"
#include <cassert>
using namespace esphome::uartex;
int main() {
  UARTExComponent c;
  c.add_rx_header({{0xf7}, {}}); c.set_rx_footer({0xee});
  c.set_rx_checksum(CHECKSUM_XOR);
  c.set_rx_data_length(0, 1, ENDIAN_BIG, -4);
  c.setup();
  std::vector<uint8_t> received;
  c.add_on_read_callback([&](const uint8_t *data, uint16_t len) { received.assign(data, data + len); });
  // Room 1 Plug 1, power field is 0x00EE (238 W).
  std::vector<uint8_t> frame = {0xf7, 0x12, 0x01, 0x1f, 0x04, 0x40, 0x11, 0x00, 0x01, 0x00, 0xee, 0x00, 0x00, 0x00, 0x00, 0x01};
  uint8_t crc = 0; for (auto b : frame) crc ^= b;
  frame.push_back(crc); frame.push_back(0xee);
  for (auto b : frame) c.bytes.push_back(b);
  c.loop();
  assert(received == frame);
}
"""
        self.compile_component(source)

    def test_cvnet_combined_climate_calls(self):
        text = (ROOT / 'cvnet/cvnet.yaml').read_text()
        block = text.split('    name: "Heater 1"', 1)[1].split('  - platform:', 1)[0]
        def body(name):
            return re.search(r'    command_' + name + r': !lambda \|-\n((?:      .*\n)+)', block)[1]
        source = """#include "climate/uartex_climate.h"
#include <cassert>
using namespace esphome;
using namespace esphome::uartex;
class Heater : public UARTExClimate { public: using UARTExClimate::control; };
int main() {
 Heater heater_1; heater_1.setup();
 #define id(x) x
 heater_1.set_command("command_heat", [&]() -> cmd_t { @HEAT@ });
 heater_1.set_command("command_off", [&]() -> cmd_t { @OFF@ });
 heater_1.set_command("command_temperature", [&](float x) -> cmd_t { @TEMP@ });
 climate::ClimateCall heat; heat.mode = climate::CLIMATE_MODE_HEAT; heat.target_temperature = 22;
 heater_1.control(heat);
 assert(heater_1.mode == climate::CLIMATE_MODE_HEAT && heater_1.target_temperature == 22);
 assert(heater_1.dequeue_tx_cmd()->data[4] == 0x96);
 assert(!heater_1.dequeue_tx_cmd());
 climate::ClimateCall target; target.target_temperature = 23;
 heater_1.control(target); assert(heater_1.dequeue_tx_cmd()->data[4] == 0x97);
 climate::ClimateCall off; off.mode = climate::CLIMATE_MODE_OFF; off.target_temperature = 24;
 heater_1.control(off); assert(heater_1.dequeue_tx_cmd()->data[4] == 24);
 assert(!heater_1.dequeue_tx_cmd());
 target.target_temperature = 25; heater_1.control(target);
 assert(heater_1.dequeue_tx_cmd()->data[4] == 25);
 assert(!heater_1.dequeue_tx_cmd());

 // A mode packet with no temperature field must still send a separate command.
 heater_1.set_command("command_heat", []() -> cmd_t { return {{0x11}}; });
 heat.target_temperature = 27;
 heater_1.control(heat);
 assert(heater_1.dequeue_tx_cmd()->data == std::vector<uint8_t>{0x11});
 assert(heater_1.dequeue_tx_cmd()->data[4] == 0x9b);
 assert(heater_1.target_temperature == 27);
 // Rejecting a mode packet must not publish its requested target.
 Heater rejected; rejected.setup();
 rejected.set_command("command_heat", [&]() -> cmd_t {
   assert(rejected.use_command_target_temperature() == 27);
   return {};
 });
 rejected.control(heat);
 assert(std::isnan(rejected.target_temperature));
 assert(!rejected.dequeue_tx_cmd());
}
""".replace('@HEAT@', body('heat')).replace('@OFF@', body('off')).replace('@TEMP@', body('temperature'))
        # Substitute markers without altering enum names.
        self.compile_climate(source)

    def compile_climate(self, source):
        with tempfile.TemporaryDirectory() as tmp:
            src, exe = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
            src.write_text(source)
            component = REPO / 'components/uartex'
            subprocess.run(['c++', '-std=c++17', '-I' + str(REPO / 'tests/native/uartex_parser/stubs'),
                            '-I' + str(component), str(component / 'uartex_device.cpp'),
                            str(component / 'climate/uartex_climate.cpp'), str(src), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_cvnet_temperature_preserves_mode(self):
        bodies = lambdas('cvnet/cvnet.yaml', 'command_temperature')
        self.assertEqual(len(bodies), 4)
        for room, body in enumerate(bodies, 1):
            self.assertIn(f'id(heater_{room}).mode == climate::CLIMATE_MODE_HEAT', body)
            expression = re.search(r'uint8_t target = (.*);', body)[1].replace(f'id(heater_{room}).mode', 'mode')
            compile_run('''#include <cstdint>
#include <cassert>
namespace climate { enum Mode { CLIMATE_MODE_OFF, CLIMATE_MODE_HEAT }; }
uint8_t target(float x, climate::Mode mode) { return EXPR; }
int main() { assert(target(22, climate::CLIMATE_MODE_OFF) == 22); assert(target(22, climate::CLIMATE_MODE_HEAT) == 0x96); }
'''.replace('EXPR', expression))


if __name__ == '__main__':
    unittest.main()
