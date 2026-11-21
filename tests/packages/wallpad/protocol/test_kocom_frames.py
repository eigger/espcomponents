"""Kocom light state filtering against interleaved room traffic."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


class KocomFrames(unittest.TestCase):
    def test_generic_preserves_hidden_third_channel(self):
        text = (ROOT / 'packages/wallpad/kocom/kocom.yaml').read_text().split('\n#Heater', 1)[0]
        for room in range(4):
            for light in (1, 2):
                block = text.split(f'id: room_{room}_light_{light}', 1)[1].split('  - platform:', 1)[0]
                expected = 'preserve_state: [9, 10]' if light == 1 else 'preserve_state: [8, 10]'
                self.assertEqual(block.count(expected), 2)

    def test_complete_status_is_used_for_preservation(self):
        mask = '[0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00]'
        for name in ('kocom.yaml', 'kocom_thinks.yaml', 'fragments/light.yaml', 'fragments/light_with_update.yaml'):
            text = (ROOT / 'packages/wallpad/kocom' / name).read_text()
            self.assertIn(mask, text)
        source = r'''#include "uartex_device.h"
#include <cassert>
using namespace esphome::uartex;
int main() {
  UARTExDevice d;
  d.set_state("state", state_t({0x30,0xd0,0,0x0e,0,1,0,0,0,0,0},
                                {0xff,0xf0,0xff,0xff,0xff,0xff,0xff,0xff,0,0,0}));
  cmd_t cmd({0x30,0xbc,0,0x0e,0,1,0,0,0xff,0,0}, {}, {}, {9,10}), out;
  assert(!d.parse_data({0x30,0xdc,0,0x0e,0})); // short ACK
  assert(!d.render_command(cmd, out));
  assert(!d.parse_data({0x30,0xdc,0,0x0e,0,1,0,0x3a,0,0,0})); // query ACK
  assert(!d.parse_data({0x30,0xdc,0,0x0e,0,2,0,0,0,0,0})); // wrong recipient
  assert(d.parse_data({0x30,0xdc,0,0x0e,0,1,0,0,0,0xff,0})); // full DC status
  assert(d.render_command(cmd, out) && out.data[9] == 0xff && out.data[10] == 0);
  assert(!d.parse_data({0x30,0xdc,0,0x0e,0,1,0,1,0,0,0})); // non-status response
  assert(d.render_command(cmd, out) && out.data[9] == 0xff);
  assert(d.parse_data({0x30,0xd0,0,0x0e,0,1,0,0,0,0,0xff}));
  assert(d.render_command(cmd, out) && out.data[9] == 0 && out.data[10] == 0xff);
}'''
        with tempfile.TemporaryDirectory() as tmp:
            src, exe = Path(tmp) / 'kocom.cpp', Path(tmp) / 'kocom'
            src.write_text(source)
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'tests/native/uartex_parser/stubs'),
                            '-I' + str(ROOT / 'components/uartex'),
                            str(ROOT / 'components/uartex/uartex_device.cpp'), str(src), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
