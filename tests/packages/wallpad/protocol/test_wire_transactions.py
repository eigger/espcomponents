"""Run package-defined transactions on the real UARTEx hub with a fake UART."""
import subprocess
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[4]
# Each row contains an OFF report, the expected outgoing ON command, and an ACK.
# These are protocol fixtures. Expected frames are independent of code under test.
CASES = [
    ('hyundai/hyundai_imazu', 'switch', 'Room 2 Plug 1',
     'f7 12 01 1f 04 40 21 00 02 00 00 00 00 00 00 01 9d ee',
     'f7 0b 01 1f 02 40 21 01 00 80 ee', 'f7 0b 01 1f 04 40 21 01 01 87 ee'),
    ('kocom/kocom', 'light', 'Room 0 Light 1',
     'aa 55 30 dc 00 0e 00 01 00 00 00 ff ff 00 00 00 00 00 19 0d 0d',
     'aa 55 30 bc 00 0e 00 01 00 00 ff ff ff 00 00 00 00 00 f8 0d 0d',
     'aa 55 30 dc 00 0e 00 01 00 00 ff ff ff 00 00 00 00 00 18 0d 0d'),
    ('samsung/samsung_sds', 'light', 'Light 1',
     'b0 79 31 00 78', 'ac 7a 01 01 56', 'b0 7a 01 01 4a'),
    ('commax/commax', 'light', 'Light 1',
     'b0 00 01 00 00 00 00 b1', '31 01 01 00 00 00 00 33', 'b1 01 01 00 00 00 00 b3'),
    ('ezville/ezville', 'light', 'light_1_1',
     'f7 0e 11 81 04 00 00 00 00 6d 08', 'f7 0e 11 41 03 01 01 00 aa 06',
     'f7 0e 11 c1 01 00 28 00'),
    ('cvnet/cvnet', 'light', 'Room 0 Light 1',
     'f7 20 01 21 9f 00 00 00 00 00 00 00 00 e1 aa',
     'f7 20 21 01 11 01 00 00 00 00 00 00 00 54 aa',
     'f7 20 01 21 9f 01 00 00 00 00 00 00 00 e2 aa'),
]


class PackageLoader(yaml.SafeLoader):
    pass


PackageLoader.add_constructor('!lambda', lambda loader, node: loader.construct_scalar(node))


def array(values):
    return '{' + ', '.join(map(str, values)) + '}'


def configure(hub, entity):
    lines = []
    headers = hub.get('rx_header', [])
    if headers and isinstance(headers[0], int):
        headers = [headers]
    for header in headers:
        lines.append('bus.add_rx_header({' + array(header) + ', {}});')
    for key in ('rx_footer', 'tx_header', 'tx_footer'):
        if key in hub:
            lines.append(f'bus.set_{key}({array(hub[key])});')
    if 'rx_data_length' in hub:
        field = hub['rx_data_length']
        lines.append(f'bus.set_rx_data_length({field["offset"]}, {field["length"]}, ENDIAN_BIG, {field["adjust"]});')
    if 'rx_length' in hub:
        length = hub['rx_length']
        if isinstance(length, str):
            length = '[](const uint8_t *data, uint16_t len) -> int16_t {' + length + '}'
        lines.append(f'bus.set_rx_length({length});')
    for direction in ('rx', 'tx'):
        for width in (1, 2):
            key = direction + ('_checksum' if width == 1 else '_checksum2')
            if key not in hub:
                continue
            value = hub[key]
            if '\n' in value:
                args = 'const uint8_t *data, uint16_t len'
                if direction == 'rx':
                    args += ', const uint8_t *header, uint16_t header_len'
                value = f'[]({args}) -> uint8_t {{ {value} }}'
            else:
                value = 'CHECKSUM_' + value.upper()
            setter = key.replace('checksum2', 'checksum_2')
            lines.append(f'bus.set_{setter}({value});')
    for key in ('state', 'state_off', 'state_on'):
        field = entity[key]
        if isinstance(field, list):
            field = {'data': field}
        lines.append(f'device.set_state("{key}", state_t({array(field["data"])}, '
                     f'{array(field.get("mask", []))}, {field.get("offset", 0)}));')
    cmd = entity['command_on']
    fields = [array(cmd.get(k, [])) for k in ('data', 'ack', 'mask', 'preserve_state')]
    lines.append(f'device.set_command("command_on", cmd_t({", ".join(fields)}));')
    return '\n'.join(lines)


class WireTransactions(unittest.TestCase):
    def test_package_wire_transactions(self):
        component = ROOT / 'components/uartex'
        source = ['''#include "uartex.h"
#include <cassert>
#include <cstdio>
using namespace esphome;
using namespace esphome::uartex;
class Device : public UARTExDevice {
 public:
  std::vector<bool> reports;
  void publish(bool state) override { reports.push_back(state); }
  bool turn_on() { return enqueue_tx_cmd(get_command_on()); }
};
class Bus : public UARTExComponent {
 public:
  bool pending() { return is_tx_cmd_pending(); }
  void receive(const std::vector<uint8_t>& frame) {
    bytes.insert(bytes.end(), frame.begin(), frame.end());
    loop();
  }
};
int main() {
''']
        for path, platform, name, report, command, ack in CASES:
            package = yaml.load((ROOT / 'packages/wallpad' / (path + '.yaml')).read_text(), Loader=PackageLoader)
            entity = next(entry for entry in package[platform] if entry['name'] == name)
            source.append(f'''
  {{
    std::fprintf(stderr, "Testing {path} / {name}\\n");
    Bus bus; Device device;
    {configure(package['uartex'], entity)}
    std::vector<std::vector<uint8_t>> received;
    bus.add_on_read_callback([&](const uint8_t *data, uint16_t size) {{
      received.emplace_back(data, data + size);
    }});
    bus.register_device(&device);
    bus.setup();
    const std::vector<uint8_t> status = {array(bytes.fromhex(report))};
    bus.receive(status);
    assert(received == std::vector<std::vector<uint8_t>>{{status}});
    assert(device.reports == std::vector<bool>{{false}});
    assert(device.turn_on());
    fake_millis += 100;
    bus.loop();
    const std::vector<uint8_t> expected = {array(bytes.fromhex(command))};
    assert(bus.writes == std::vector<std::vector<uint8_t>>{{expected}});
    assert(bus.pending());
    const std::vector<uint8_t> valid_ack = {array(bytes.fromhex(ack))};
    auto corrupt_ack = valid_ack;
    corrupt_ack[corrupt_ack.size() - {1 + len(package["uartex"].get("rx_footer", []))}] ^= 1;
    bus.receive(corrupt_ack);
    assert(bus.pending());
    bus.receive(valid_ack);
    assert((received == std::vector<std::vector<uint8_t>>{{status, valid_ack}}));
    assert(!bus.pending());
    fake_millis += 1000;
    bus.loop();
    assert(bus.writes.size() == 1);
  }}''')
        source.append('}')
        with tempfile.TemporaryDirectory() as tmp:
            cpp, exe = Path(tmp) / 'transactions.cpp', Path(tmp) / 'transactions'
            cpp.write_text('\n'.join(source))
            subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT / 'tests/native/uartex_parser/stubs'),
                            '-I' + str(component), *(str(component / file) for file in
                            ('uartex.cpp', 'uartex_device.cpp', 'parser.cpp', 'checksum.cpp')),
                            str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
