"""Config-validation tests for gps_compass: every rejected configuration must fail `esphome config` with its message.

Run: python tests/native/gps_compass_config/test_invalid_configs.py [-k substring] [-j jobs]
Requires the esphome Python package.  Each case is (name, gps_compass options, actions, expected message part); the
first cases are positive controls (expected message None).  The valid compile test lives in tests/components/gps_compass.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
COMPONENTS = ROOT / "components"

BASE = """
esphome:
  name: gps-compass-invalid
esp32:
  board: esp32dev
  framework:
    type: esp-idf
external_components:
  - source:
      type: local
      path: {components}
    components: [ gps_compass ]
logger:
uart:
  id: uart_gps
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600
gps:
  id: id_gps
  uart_id: uart_gps
sensor:
  - platform: template
    id: sx
  - platform: template
    id: sy
  - platform: template
    id: sz
gps_compass:
  id: compass
{gps_id}{options}
interval:
  - interval: 1s
    then:
{actions}
"""

NOOP = "- gps_compass.clear_level:"
GPS_ID = "  gps_id: id_gps\n"
XYZ = "  magnetometer: {x: sx, y: sy, z: sz}"

CASES = [
    ("minimal (positive control)", "", NOOP, None),
    ("magnetometer and accelerometer (positive control)", XYZ + "\n  accelerometer: {x: sx, y: sy, z: sz}", NOOP, None),
    ("speed with unit (positive control)", "  min_learning_speed: 25 km/h\n  moving_speed: 10 kph", NOOP, None),
    ("reverse lambda (positive control)", '  reverse: !lambda "return false;"', NOOP, None),
    ("set_level long form (positive control)", "", "- gps_compass.set_level: { pitch: 1.0, roll: 2.0 }", None),
    ("set_level with no options (positive control)", "", "- gps_compass.set_level:", None),
    ("missing gps_id", "", NOOP, "'gps_id' is a required option"),
    ("gps_id is not a gps", "  gps_id: uart_gps", NOOP, "uart_gps"),
    ("unknown gps_id", "  gps_id: nope", NOOP, "Couldn't find ID 'nope'"),
    ("magnetometer missing z", "  magnetometer: {x: sx, y: sy}", NOOP, "'z' is a required option"),
    ("magnetometer sensor unknown", "  magnetometer: {x: sx, y: sy, z: nope}", NOOP, "Couldn't find ID 'nope'"),
    ("accelerometer missing x", "  accelerometer: {y: sy, z: sz}", NOOP, "'x' is a required option"),
    ("forward_axis invalid", "  forward_axis: W", NOOP, "forward_axis"),
    ("same forward and right axis", "  forward_axis: Y\n  right_axis: -Y", NOOP, "must be different axes"),
    ("default axes collide after override", "  forward_axis: X", NOOP, "must be different axes"),
    ("pitch_offset out of range", "  pitch_offset: 120", NOOP, "pitch_offset"),
    ("roll_offset not a number", "  roll_offset: left", NOOP, "roll_offset"),
    ("reverse not boolean", "  reverse: maybe", NOOP, "reverse"),
    ("negative speed", "  min_learning_speed: -1", NOOP, "min_learning_speed"),
    ("moving below stopped", "  moving_speed: 2\n  stopped_speed: 3\n  min_learning_speed: 18", NOOP, "must not be below 'stopped_speed'"),
    ("learning speed below moving speed", "  min_learning_speed: 5", NOOP, "must not be below 'moving_speed'"),
    ("max_hdop zero", "  max_hdop: 0", NOOP, "max_hdop"),
    ("min_satellites zero", "  min_satellites: 0", NOOP, "min_satellites"),
    ("field_filter above 1", "  field_filter: 1.5", NOOP, "field_filter"),
    ("field_filter zero", "  field_filter: 0", NOOP, "field_filter"),
    ("min_sectors above 8", "  min_sectors: 9", NOOP, "min_sectors"),
    ("min_sectors zero", "  min_sectors: 0", NOOP, "min_sectors"),
    ("max_sector_gap zero", "  max_sector_gap: 0", NOOP, "max_sector_gap"),
    ("negative sensor_timeout", "  sensor_timeout: -1s", NOOP, "sensor_timeout"),
    ("heading sensor wrong type", "  heading: 5", NOOP, "heading"),
    ("unknown option", "  figure_eight: true", NOOP, "[figure_eight] is an invalid option"),
    ("set_level pitch out of range", "", "- gps_compass.set_level: { pitch: 100 }", "pitch"),
    ("set_level unknown id", "", "- gps_compass.set_level: { id: nope }", "Couldn't find ID 'nope'"),
    ("reset_calibration unknown id", "", "- gps_compass.reset_calibration: nope", "Couldn't find ID 'nope'"),
]


def run(case):
    name, options, actions, expected = case
    text = BASE.format(
        components=COMPONENTS,
        gps_id="" if "gps_id" in options or name == "missing gps_id" else GPS_ID,
        options=options,
        actions="      " + actions.replace("\n", "\n      "),
    )
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "test.yaml"
        path.write_text(text)
        proc = subprocess.run(["esphome", "config", str(path)], capture_output=True, text=True, cwd=tmp)
    out = proc.stdout + proc.stderr
    if expected is None:
        ok = proc.returncode == 0
    else:
        ok = proc.returncode != 0 and expected in out
    return name, ok, out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-k", default="")
    parser.add_argument("-j", type=int, default=4)
    args = parser.parse_args()
    cases = [c for c in CASES if args.k in c[0]]
    failed = 0
    with ThreadPoolExecutor(args.j) as pool:
        for name, ok, out in pool.map(run, cases):
            print(("PASS " if ok else "FAIL ") + name)
            if not ok:
                failed += 1
                print("\n".join("    " + line for line in out.strip().splitlines()[-15:]))
    print(f"{len(cases) - failed}/{len(cases)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
