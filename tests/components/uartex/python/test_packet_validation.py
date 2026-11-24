import importlib.util
import logging
from pathlib import Path
import unittest
from unittest.mock import patch
import sys
import types

path = Path(__file__).resolve().parents[4] / "components/uartex/packet_validation.py"
package = types.ModuleType("packet_validation_test")
package.__path__ = [str(path.parent)]
sys.modules[package.__name__] = package
spec = importlib.util.spec_from_file_location("packet_validation_test.packet_validation", path)
validation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validation)


class PacketValidationTests(unittest.TestCase):
    def warnings(self, hub, entity=None):
        with self.assertLogs("packet_validation_test.packet_validation", logging.WARNING) as output:
            validation.validate_packets(hub, {"binary_sensor": [entity]} if entity else {})
        return "\n".join(output.output)

    def no_warnings(self, hub, entity=None):
        with patch.object(validation._LOGGER, "warning") as warning:
            validation.validate_packets(hub, {"binary_sensor": [entity]} if entity else {})
            warning.assert_not_called()

    def test_masks_for_header_state_and_ack(self):
        hub = {"id": "bus", "rx_header": [{"data": [0x81], "mask": [0xF0]}]}
        entity = {"uartex_id": "bus", "name": "Door", "state_on": {"data": [0x02], "mask": [0x01]},
                  "command_on": {"ack": [0x04], "mask": [0x03]}}
        warnings = self.warnings(hub, entity)
        self.assertIn("rx_header[0] byte 0", warnings)
        self.assertIn("Door.state_on byte 0", warnings)
        self.assertIn("Door.command_on.ack byte 0", warnings)

    def test_multiple_headers_and_trailer_width(self):
        hub = {"id": "bus", "rx_length": 8, "rx_header": [{"data": [1, 2]}, {"data": [1]}],
               "rx_footer": [9], "rx_checksum2": "XOR"}
        entity = {"uartex_id": "bus", "state_on": {"data": [1, 2, 3, 4, 5]},
                  "command_on": {"ack": [1, 2, 3, 4, 5]}}
        warnings = self.warnings(hub, entity)
        self.assertIn("payload lengths are [3, 4]", warnings)
        self.assertIn("command_on.ack", warnings)
        entity["state_on"]["data"] = [1, 2, 3, 4]
        entity["command_on"]["ack"] = [1, 2, 3, 4]
        self.no_warnings(hub, entity)

    def test_exact_numeric_and_inverted(self):
        hub = {"id": "bus", "rx_length": 4}
        entity = {"uartex_id": "bus", "state_on": {"data": [1], "match": "EXACT"},
                  "state_number": {"offset": 3, "length": 2}}
        warnings = self.warnings(hub, entity)
        self.assertIn("exact payload length 1", warnings)
        self.assertIn("numeric field will be truncated", warnings)
        entity["state_on"]["inverted"] = True
        self.assertIn("exact payload length 1", self.warnings(hub, entity))
        entity["state_on"]["match"] = "PREFIX"
        entity["state_on"]["mask"] = [0]
        entity["state_number"]["length"] = 1
        self.no_warnings(hub, entity)

    def test_variable_length_and_other_hub(self):
        hub = {"id": "bus", "rx_length": object()}
        entity = {"uartex_id": "bus", "state_on": {"data": [1, 2], "offset": 128}}
        self.no_warnings(hub, entity)
        hub["rx_length"] = 1
        entity["uartex_id"] = "other"
        self.no_warnings(hub, entity)

    def test_header_candidates_and_minimum_payload(self):
        hub = {"id": "bus", "rx_length": 4, "rx_header": [
            {"data": []}, {"data": [0x81], "mask": [0xF0]}, {"data": [1, 2]}]}
        entity = {"uartex_id": "bus", "state_on": {"data": [1, 2, 3]}}
        warnings = self.warnings(hub, entity)
        self.assertIn("payload lengths are [2]", warnings)
        hub["rx_length"] = 2
        warnings = self.warnings(hub, entity)
        self.assertIn("cannot fit a viable header", warnings)
        self.assertNotIn("needs at least payload", warnings)
        hub["rx_header"] = [{"data": []}]
        hub["rx_length"] = 1
        entity["state_on"]["data"] = [1]
        self.no_warnings(hub, entity)

    def test_shadowed_header_order_and_partial_overlap(self):
        short = {"data": [0xAA]}
        long = {"data": [0xAA, 0x55]}
        hub = {"id": "bus", "rx_length": 4, "rx_header": [short, long]}
        entity = {"uartex_id": "bus", "state_on": {"data": [1, 2], "match": "EXACT"}}
        self.assertIn("payload lengths are [3]", self.warnings(hub, entity))
        hub["rx_header"] = [long, short]
        self.no_warnings(hub, entity)
        # Only some matches of the second candidate match the first.
        hub["rx_header"] = [{"data": [0xA0], "mask": [0xF0]},
                            {"data": [0x80, 0x55], "mask": [0x80]}]
        self.no_warnings(hub, entity)

    def test_shadowed_header_masks_and_viability(self):
        hub = {"id": "bus", "rx_length": 4, "rx_header": [
            {"data": [0xA0], "mask": [0xF0]},
            {"data": [0xAB, 0x55]}]}
        entity = {"uartex_id": "bus", "state_on": {"data": [1, 2], "match": "EXACT"}}
        self.assertIn("payload lengths are [3]", self.warnings(hub, entity))
        # A constrained earlier candidate cannot shadow an unconstrained later one.
        hub["rx_header"] = [{"data": [0xAB]},
                            {"data": [0xA0, 0x55], "mask": [0xF0]}]
        self.no_warnings(hub, entity)
        # An impossible earlier candidate cannot shadow any later one.
        hub["rx_header"] = [{"data": [0xA1], "mask": [0xF0]},
                            {"data": [0xA1, 0x55]}]
        warnings = self.warnings(hub, entity)
        self.assertIn("rx_header[0]", warnings)
        self.assertNotIn("payload length", warnings)

    def test_numeric_field_missing(self):
        hub = {"id": "bus", "rx_length": 2}
        entity = {"uartex_id": "bus", "state_number": {"offset": 2, "length": 1}}
        self.assertIn("numeric field will be entirely missing", self.warnings(hub, entity))

    def test_reply_and_short_mask(self):
        hub = {"id": "bus", "rx_length": 1, "rx_reply": [{"state": {"data": [1, 2], "mask": [1]}, "command": object()}]}
        self.assertIn("rx_reply[0].state", self.warnings(hub))
        hub["rx_length"] = 2
        self.no_warnings(hub)


class DebounceMaskValidationTests(unittest.TestCase):
    logger = "packet_validation_test.packet_validation"

    @staticmethod
    def debounce(ms, offset, mask):
        return {"delay": types.SimpleNamespace(total_milliseconds=ms), "offset": offset, "mask": mask}

    def warn(self, debounce):
        with self.assertLogs(self.logger, logging.WARNING) as output:
            validation._debounce(debounce, "x", [4])
        return "\n".join(output.output)

    def silent(self, debounce):
        with self.assertNoLogs(self.logger, logging.WARNING):
            validation._debounce(debounce, "x", [4])

    def test_mask_entirely_outside_payload(self):
        self.assertIn("entirely outside", self.warn(self.debounce(2000, 4, [0x01])))

    def test_mask_partly_outside_payload(self):
        self.assertIn("partly outside", self.warn(self.debounce(2000, 3, [0x01, 0x01])))

    def test_mask_in_range_or_ignored_is_silent(self):
        self.silent(self.debounce(2000, 2, [0x01, 0x01]))
        self.silent(self.debounce(2000, 0, []))
        self.silent(self.debounce(0, 4, [0x01]))


if __name__ == "__main__":
    unittest.main()
