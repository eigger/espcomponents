"""Config-validation tests for ui_sound: every rejected configuration must fail `esphome config` with its message.

Run: python tests/native/ui_sound_config/test_invalid_configs.py [-k substring] [-j jobs]
Requires the esphome Python package.  Each case is (name, ui_sound block, actions, expected message part); the first
cases are positive controls (expected message None).  The valid compile test lives in tests/components/ui_sound.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
COMPONENTS = ROOT / "components"
WAV = ROOT / "tests" / "components" / "ui_sound"

BASE = """
esphome:
  name: ui-sound-invalid
esp32:
  board: esp32dev
  framework:
    type: esp-idf
external_components:
  - source:
      type: local
      path: {components}
    components: [ ui_sound ]
logger:
wifi:
  ssid: test
  password: testtest123
i2s_audio:
  i2s_lrclk_pin: GPIO25
  i2s_bclk_pin: GPIO26
speaker:
  - platform: i2s_audio
    id: ui_speaker
    dac_type: external
    i2s_dout_pin: GPIO22
    channel: mono
    sample_rate: 16000
media_player:
  - platform: speaker
    id: ui_sound_player
    announcement_pipeline:
      speaker: ui_speaker
      format: WAV
      sample_rate: 16000
      num_channels: 1
    files:
      - id: sound_click
        file: {wav}/click.wav
ui_sound:
  media_player: ui_sound_player
{block}
interval:
  - interval: 1s
    then:
{actions}
"""

OK_CUES = "  cues:\n    - id: click\n      file: sound_click"

CASES = [
    ("minimal (positive control)", OK_CUES, "- ui_sound.play: click", None),
    ("long form play (positive control)", OK_CUES, "- ui_sound.play: { cue: click, force: true }", None),
    ("skip_when_muted (positive control)", OK_CUES + "\n  skip_when_muted: true", "- ui_sound.stop:", None),
    ("skip_when_muted must be boolean", OK_CUES + "\n  skip_when_muted: maybe", "- ui_sound.stop:", "skip_when_muted"),
    ("unknown cue id in play", OK_CUES, "- ui_sound.play: nope", "Couldn't find ID 'nope'"),
    ("a media file id is not a cue id", OK_CUES, "- ui_sound.play: sound_click", "'sound_click' of type"),
    ("unknown media file", "  cues:\n    - id: click\n      file: nope", "- ui_sound.stop:", "Couldn't find ID 'nope'"),
    ("priority above 255", OK_CUES + "\n      priority: 256", "- ui_sound.stop:", "priority"),
    ("negative priority", OK_CUES + "\n      priority: -1", "- ui_sound.stop:", "priority"),
    ("duplicate cue ids", OK_CUES + "\n    - id: click\n      file: sound_click", "- ui_sound.stop:", "cue ids must be unique"),
    ("no cues", "  cues: []", "- ui_sound.stop:", "at least 1"),
    ("missing cues", "", "- ui_sound.stop:", "'cues' is a required option"),
    ("media_player is not a speaker player", OK_CUES + "\n  media_player: ui_speaker", "- ui_sound.stop:", "ui_speaker"),
    ("max_busy zero", OK_CUES + "\n  max_busy: 0ms", "- ui_sound.stop:", "at least 1ms"),
    ("negative cooldown", OK_CUES + "\n      cooldown: -1s", "- ui_sound.stop:", "cooldown"),
    ("unknown option", OK_CUES + "\n  queue: true", "- ui_sound.stop:", "[queue] is an invalid option"),
    ("force must be boolean", OK_CUES, "- ui_sound.play: { cue: click, force: maybe }", "force"),
]


def run(case):
    name, block, actions, expected = case
    text = BASE.format(
        components=COMPONENTS, wav=WAV, block=block, actions="      " + actions.replace("\n", "\n      ")
    )
    if "media_player: ui_speaker" in block:  # override the base's player instead of a duplicate key
        text = text.replace("  media_player: ui_sound_player\n", "")
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
