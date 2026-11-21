"""Focused regression checks for the wallpad package packet definitions."""
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


def package(path):
    return (ROOT / 'packages' / 'wallpad' / path).read_text()


class WallpadPackets(unittest.TestCase):
    def test_hyundai_heater_status_and_control_variants(self):
        text = package('hyundai/hyundai_imazu.yaml')
        for room in range(0x11, 0x16):
            expected = f'data: [0x00, 0x01, 0x18, 0x04, 0x44, 0x{room:02x}]\n      mask: [0x00, 0xff, 0xff, 0xff, 0xfc, 0xff]'
            self.assertIn(expected, text)
            for opcode in (0x45, 0x46):
                self.assertEqual(opcode & 0xfc, 0x44)

    def test_ezville_half_degree_encoder(self):
        text = package('ezville/ezville.yaml')
        self.assertIn('mask: [0x01]\n      data: [0x01]  # bit0=1', text)
        expressions = re.findall(r'uint8_t target = (.*);', text)
        expressions = [e for e in expressions if '0x80' in e and 'x' in e]
        self.assertEqual(len(expressions), 4)
        self.assertEqual(len(set(expressions)), 1)
        source = '''#include <cstdint>\n#include <cassert>\nuint8_t encode(float x) { return EXPR; }\nint main() { assert(encode(25.5f)==0x99); assert(encode(26.0f)==0x1a); assert(encode(26.5f)==0x9a); assert(encode(30.5f)==0x9e); }\n'''.replace('EXPR', expressions[0])
        with tempfile.TemporaryDirectory() as tmp:
            src, exe = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
            src.write_text(source)
            subprocess.run(['c++', '-std=c++11', str(src), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_cvnet_heaters_and_floor(self):
        text = package('cvnet/cvnet.yaml')
        for room in range(1, 5):
            block = text.split(f'    name: "Heater {room}"', 1)[1].split('  - platform:', 1)[0]
            self.assertEqual(block.count(f'id(heater_{room}).use_command_target_temperature()'), 2)
            self.assertEqual(block.count('<= 30)) return {};'), 2)
        self.assertIn('offset: 10\n      length: 2\n      decode: ascii', text)

    def test_kocom_light_commands_preserve_sibling_channels(self):
        for fragment in ('light.yaml', 'light_with_update.yaml'):
            text = package('kocom/fragments/' + fragment)
            self.assertEqual(text.count('preserve_state:'), 2)
            self.assertNotIn('last_state(', text)
        for name, count in (('kocom.yaml', 16), ('kocom_thinks.yaml', 6)):
            text = package('kocom/' + name).split('\n#Heater', 1)[0]
            self.assertEqual(text.count('preserve_state:'), count)
            self.assertNotIn('current_values.is_on()', text)

    def test_kocom_heater_targets_checked_before_conversion(self):
        for name in ('kocom.yaml', 'kocom_thinks.yaml', 'fragments/heater.yaml'):
            text = package('kocom/' + name)
            targets = re.findall(r'float target = (?:id\([^\n]+\)\.(?:target_temperature|use_command_target_temperature\(\))|x);\n(\s+)([^\n]+)', text)
            self.assertTrue(targets)
            for _, guard in targets:
                self.assertEqual(guard, 'if (!(target >= 5 && target <= 30)) return {};')

    def test_samsung_common_framing(self):
        standalone = package('samsung/samsung_sds.yaml')
        shared = package('samsung/samsung_sds_shared.yaml')
        match = re.search(r'  rx_length: !lambda \|-\n((?:    .*\n)+)', standalone)
        self.assertIsNotNone(match)
        body = match.group(1)
        source = """#include <cstdint>
#include <cassert>
int frame_length(const uint8_t *data, uint16_t len) {
BODY
}
int main() {
  uint8_t frame[] = {0xB0, 0x79};
  assert(frame_length(frame, 0) == 0);
  assert(frame_length(frame, 1) == 0);
  const uint8_t opcodes[] = {0x55, 0x79, 0x7a, 0x4e, 0x4f, 0x6f, 0x7c, 0x7d, 0x7f, 0x4a, 0x4b, 0x6e};
  const int expected[] = {4, 5, 5, 6, 6, 7, 8, 8, 8, 10, 10, 10};
  for (unsigned i = 0; i < sizeof(opcodes); ++i) {
    frame[1] = opcodes[i];
    assert(frame_length(frame, 2) == expected[i]);
    frame[0] = 0xa4;
    assert(frame_length(frame, 2) == 4);
    frame[0] = 0xb0;
  }
}
""".replace('BODY', body)
        with tempfile.TemporaryDirectory() as tmp:
            src, exe = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
            src.write_text(source)
            subprocess.run(['c++', '-std=c++11', str(src), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
        self.assertNotIn('  rx_length:', shared)
        self.assertIn('!include samsung_sds.yaml', shared)


if __name__ == '__main__':
    unittest.main()
