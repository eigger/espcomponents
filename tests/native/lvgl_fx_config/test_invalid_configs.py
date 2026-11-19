"""Config-validation tests for lvgl_fx: every rejected configuration must fail `esphome config` with its message.

Run: python tests/native/lvgl_fx_config/test_invalid_configs.py [-k substring] [-j jobs]
Requires the esphome Python package (the one the component is built with).

The CI compiles every tests/components/<component>/test.*.yaml; the negative cases live here instead, as snippets
spliced into a small valid base configuration.  Each case is (name, lvgl_fx block, actions, expected message part);
`actions` are lvgl_fx.* actions run from an interval.  The first case is a positive control: the base itself is valid.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import textwrap

COMPONENTS = Path(__file__).resolve().parents[3] / "components"

BASE = """
esphome:
  name: fx-invalid
esp32:
  board: esp32dev
  framework:
    type: esp-idf
external_components:
  - source:
      type: local
      path: {components}
    components: [ lvgl_fx ]
logger:
spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
display:
  - platform: mipi_spi
    id: main_display
    model: ILI9341
    cs_pin: GPIO15
    dc_pin: GPIO27
    reset_pin: GPIO33
    invert_colors: false
    update_interval: never
lvgl:
  displays: [ main_display ]
  pages:
    - id: main_page
      widgets:
        - label:
            id: fx_label
            text: "0"
        - bar:
            id: fx_bar
lvgl_fx:
{fx}
interval:
  - interval: 1s
    then:
{actions}
"""

VALID_EFFECT = """
  effects:
    - id: ok
      tracks:
        - pulse:
"""

# (name, lvgl_fx block, actions, expected part of the error)
CASES = [
    ("instant is not a play option", VALID_EFFECT, "- lvgl_fx.play: { id: fx_label, effect: ok, instant: true }", "[instant] is an invalid option"),
    ("duplicate widget property binding", "  transitions:\n    custom:\n      - id: preset_a\n        text: { duration: 10ms }\n    bindings:\n      - { id: fx_label, text: preset_a }\n      - { id: fx_label, text: preset_a }", "", "Duplicate transition binding"),
    ("unknown binding widget", "  transitions:\n    custom:\n      - id: preset_a\n        text: { duration: 10ms }\n    bindings:\n      - { id: missing_widget, text: preset_a }", "", "Couldn't find ID 'missing_widget'"),
    ("unknown binding transition", "  transitions:\n    bindings:\n      - { id: fx_label, text: missing_preset }", "", "Couldn't find ID 'missing_preset'"),
    ("valid base (positive control)", VALID_EFFECT, "- lvgl_fx.play: { id: fx_label, effect: ok }", None),
    (
        "empty lvgl_fx: block (quick start)",
        "  # nothing configured",
        "- lvgl_fx.play: { id: fx_label, effect: pulse }",
        None,
    ),
    (
        "id defaults is fine (positive control)",
        "  transitions:\n    defaults:\n      value: { duration: 100ms }\n    custom:\n      - id: defaults\n"
        "        value: { duration: 50ms }\n  effects:\n    - id: defaults_def\n      tracks:\n        - pulse:",
        "",
        None,
    ),
    (
        "format with a non-ASCII digit width",
        "  transitions:\n    defaults:\n      text: { format: '%\u0663f' }",
        "",
        "is not a floating conversion",
    ),
    (
        "format with a non-ASCII digit width and precision",
        "  transitions:\n    defaults:\n      text: { format: '%\u0661\u0662.1f' }",
        "",
        "is not a floating conversion",
    ),
    (
        "format with a non-ASCII digit precision",
        "  transitions:\n    defaults:\n      text: { format: '%.\u0662f' }",
        "",
        "is not a floating conversion",
    ),
    (
        "infinite percentage",
        "  presets:\n    pulse: { scale: .inf }",
        "",
        "is not a finite percentage",
    ),
    (
        "nan percentage",
        "  presets:\n    glow: { opacity: .nan }",
        "",
        "is not a finite percentage",
    ),
    (
        "effect id fx",
        "  effects:\n    - id: fx\n      tracks:\n        - pulse:",
        "",
        "'fx' is reserved",
    ),
    (
        "transition id fx",
        "  transitions:\n    custom:\n      - id: fx\n        value: { duration: 50ms }",
        "",
        "'fx' is reserved",
    ),
    (
        "component id fx",
        "  id: fx",
        "",
        "'fx' is reserved",
    ),
    # ---- then ----
    (
        "then on an infinite loop",
        """
  effects:
    - id: looper
      repeat: infinite
      then: pulse
      tracks:
        - blink:
""",
        "",
        "'then' cannot be used with 'repeat: infinite'",
    ),
    (
        "then pointing at a loop (effect)",
        """
  effects:
    - id: a
      then: looper
      tracks:
        - pulse:
    - id: looper
      repeat: infinite
      tracks:
        - blink:
""",
        "",
        "is a loop (repeat: infinite), which never ends; it cannot follow an effect",
    ),
    (
        "then pointing at a loop (transition)",
        """
  transitions:
    custom:
      - id: t
        value: { duration: 100ms }
        then: looper
  effects:
    - id: looper
      repeat: infinite
      tracks:
        - blink:
""",
        "",
        "it cannot follow a transition",
    ),
    (
        "then chain that loops back",
        """
  effects:
    - id: a
      then: b
      tracks:
        - pulse:
    - id: b
      then: a
      tracks:
        - pulse:
""",
        "",
        "'then' chain loops back to itself: a -> b -> a",
    ),
    (
        "then pointing at itself",
        """
  effects:
    - id: a
      then: a
      tracks:
        - pulse:
""",
        "",
        "'then' chain loops back to itself: a -> a",
    ),
    (
        "unknown effect id in then",
        """
  effects:
    - id: a
      then: nope
      tracks:
        - pulse:
""",
        "",
        "Couldn't find ID 'nope'",
    ),
    (
        "unknown effect in play",
        VALID_EFFECT,
        "- lvgl_fx.play: { id: fx_label, effect: nope }",
        "Couldn't find ID 'nope'",
    ),
    (
        "unknown effect in stop",
        VALID_EFFECT,
        "- lvgl_fx.stop: { id: fx_label, effect: nope }",
        "Couldn't find ID 'nope'",
    ),
    (
        "unknown transition in set",
        VALID_EFFECT,
        "- lvgl_fx.set: { id: fx_label, text: x, transition: nope }",
        "Couldn't find ID 'nope'",
    ),
    (
        "an effect id is not a transition id",
        VALID_EFFECT,
        "- lvgl_fx.set: { id: fx_label, text: x, transition: ok }",
        "doesn't inherit from lvgl_fx::TransitionDef",
    ),
    ("unknown widget in set", VALID_EFFECT, "- lvgl_fx.set: { id: nope, visible: true }", "Couldn't find ID 'nope'"),
    # ---- styles ----
    (
        "slide_up on text",
        "  transitions:\n    defaults:\n      text: { style: slide_up }",
        "",
        "Unknown value 'slide_up'",
    ),
    (
        "count on visible",
        "  transitions:\n    defaults:\n      visible: { style: count }",
        "",
        "Unknown value 'count'",
    ),
    (
        "roll on visible (show)",
        "  transitions:\n    defaults:\n      visible: { show: roll, hide: fade }",
        "",
        "Unknown value 'roll'",
    ),
    (
        "typewriter is not available",
        "  transitions:\n    defaults:\n      text: { style: typewriter }",
        "",
        "Unknown value 'typewriter'",
    ),
    (
        "style on a property without styles",
        "  transitions:\n    defaults:\n      value: { style: fade }",
        "",
        "[style] is an invalid option for [value]",
    ),
    (
        "format on a property without text",
        "  transitions:\n    defaults:\n      visible: { format: '%.0f' }",
        "",
        "[format] is an invalid option for [visible]",
    ),
    (
        "style together with show",
        "  transitions:\n    defaults:\n      visible: { style: fade, show: fade, hide: fade }",
        "",
        "Use either 'style'",
    ),
    (
        "show without hide",
        "  transitions:\n    defaults:\n      visible: { show: slide_up }",
        "",
        "'show' needs 'hide'",
    ),
    (
        "transition that sets nothing",
        "  transitions:\n    custom:\n      - id: empty",
        "",
        "sets nothing",
    ),
    # ---- format (snprintf with one double) ----
    ("format %d", "  transitions:\n    defaults:\n      text: { format: '%d' }", "", "is not a floating conversion"),
    ("format %s", "  transitions:\n    defaults:\n      text: { format: 'x %s' }", "", "is not a floating conversion"),
    ("format %*f", "  transitions:\n    defaults:\n      text: { format: '%*f' }", "", "is not a floating conversion"),
    ("format %lf", "  transitions:\n    defaults:\n      text: { format: '%lf' }", "", "is not a floating conversion"),
    ("format %n", "  transitions:\n    defaults:\n      text: { format: '%n' }", "", "is not a floating conversion"),
    (
        "format dangling %",
        "  transitions:\n    defaults:\n      text: { format: '50%' }",
        "",
        "is not a floating conversion",
    ),
    (
        "format with two floats",
        "  transitions:\n    defaults:\n      text: { format: '%f %f' }",
        "",
        "exactly one floating conversion",
    ),
    (
        "format without a conversion",
        "  transitions:\n    defaults:\n      text: { format: 'abc %%' }",
        "",
        "exactly one floating conversion",
    ),
    (
        "format precision 7",
        "  transitions:\n    defaults:\n      text: { format: '%.7f' }",
        "",
        "precision 7 is too large",
    ),
    ("format width 41", "  transitions:\n    defaults:\n      text: { format: '%41f' }", "", "width 41 is too large"),
    (
        "format in a count transition",
        "  transitions:\n    custom:\n      - id: c\n        text: { style: count, format: '%i' }",
        "",
        "is not a floating conversion",
    ),
    # ---- ranges ----
    (
        "distance 1001",
        "  transitions:\n    defaults:\n      visible: { style: slide_up, distance: 1001 }",
        "",
        "must be at most 1000",
    ),
    (
        "visible from 0%",
        "  transitions:\n    defaults:\n      visible: { style: zoom, from: 0% }",
        "",
        "must be between 1% and 200%",
    ),
    (
        "text from 101%",
        "  transitions:\n    defaults:\n      text: { from: 101% }",
        "",
        "must be between 0% and 100%",
    ),
    (
        "percent without a percent sign",
        "  presets:\n    pulse: { scale: 112 }",
        "",
        "write percentages like '112%'",
    ),
    ("fractional percent", "  presets:\n    pulse: { scale: 112.5% }", "", "is not a whole percent"),
    (
        "duration above 60 s",
        "  transitions:\n    defaults:\n      value: { duration: 61s }",
        "",
        "must be at most",
    ),
    ("negative duration", "  transitions:\n    defaults:\n      value: { duration: -1s }", "", "must be at least 0s"),
    ("unknown curve", "  transitions:\n    defaults:\n      value: { curve: bounce }", "", "Unknown value 'bounce'"),
    (
        "shake cycles 0",
        "  effects:\n    - id: a\n      tracks:\n        - shake: { cycles: 0 }",
        "",
        "cycles: 0 is invalid",
    ),
    (
        "shake cycles 0 in a preset",
        "  presets:\n    shake: { cycles: 0 }",
        "",
        "cycles: 0 is invalid",
    ),
    (
        "shake cycles 101",
        "  effects:\n    - id: a\n      tracks:\n        - shake: { cycles: 101 }",
        "",
        "must be at most 100",
    ),
    (
        "nudge distance beyond 1000",
        "  effects:\n    - id: a\n      tracks:\n        - nudge: { x: 1001 }",
        "",
        "must be at most 1000",
    ),
    (
        "preset distance beyond 1000",
        "  presets:\n    nudge_up: { distance: 1001 }",
        "",
        "must be at most 1000",
    ),
    (
        "effect longer than 65535 ms",
        "  effects:\n    - id: a\n      tracks:\n        - blink: { delay: 60s, duration: 6s }",
        "",
        "is longer than 65535 ms",
    ),
    (
        "effect longer than 65535 ms (motion token)",
        "  motion:\n    slow: 10s\n  effects:\n    - id: a\n      tracks:\n"
        "        - blink: { delay: 60s, duration: slow }",
        "",
        "is longer than 65535 ms",
    ),
    ("repeat 0", "  effects:\n    - id: a\n      repeat: 0\n      tracks:\n        - pulse:", "", "must be at least 1"),
    (
        "repeat infinite as a global default",
        "  effect_defaults:\n    repeat: infinite",
        "",
        "Expected integer",
    ),
    (
        "cooldown above 60 s",
        "  effects:\n    - id: a\n      cooldown: 61s\n      tracks:\n        - pulse:",
        "",
        "must be at most",
    ),
    (
        "flash color out of range",
        "  effects:\n    - id: a\n      tracks:\n        - flash: { color: 0x1000000 }",
        "",
        "must be between 0x000000 and 0xFFFFFF",
    ),
    ("opacity above 100%", "  presets:\n    glow: { opacity: 101% }", "", "must be between 0% and 100%"),
    # ---- structure ----
    ("effect without tracks", "  effects:\n    - id: a\n      tracks: []", "", "length of value must be at least 1"),
    (
        "two materials in one track",
        "  effects:\n    - id: a\n      tracks:\n        - pulse:\n          blink:",
        "",
        "A track is a single material",
    ),
    (
        "unknown material",
        "  effects:\n    - id: a\n      tracks:\n        - wobble:",
        "",
        "Unknown value 'wobble'",
    ),
    (
        "nudge without a direction",
        "  effects:\n    - id: a\n      tracks:\n        - nudge: { duration: fast }",
        "",
        "A nudge needs 'x' and/or 'y'",
    ),
    (
        "field of another material",
        "  effects:\n    - id: a\n      tracks:\n        - pulse: { cycles: 2 }",
        "",
        "[cycles] is an invalid option for [pulse]",
    ),
    (
        "effect id of a built-in",
        "  effects:\n    - id: pulse\n      tracks:\n        - blink:",
        "",
        "is the name of a built-in effect",
    ),
    ("unknown preset", "  presets:\n    wobble: { scale: 110% }", "", "[wobble] is an invalid option"),
    (
        "nudge preset with x",
        "  presets:\n    nudge_up: { x: 3 }",
        "",
        "[x] is an invalid option for [nudge_up]",
    ),
    ("empty preset", "  presets:\n    pulse:", "", "Preset 'pulse' sets nothing"),
    ("unknown retrigger", "  effect_defaults:\n    retrigger: always", "", "Unknown value 'always'"),
    # ---- actions ----
    ("set without a property", VALID_EFFECT, "- lvgl_fx.set: { id: fx_label }", "Must contain exactly one of"),
    (
        "set with two properties",
        VALID_EFFECT,
        "- lvgl_fx.set: { id: fx_label, x: 1, y: 2 }",
        "Cannot specify more than one of",
    ),
    ("set_enabled with a non-bool", VALID_EFFECT, "- lvgl_fx.set_enabled: banana", "Expected boolean value"),

    ("play without an effect", VALID_EFFECT, "- lvgl_fx.play: { id: fx_label }", "'effect' is a required option"),
    (
        "set visible with a number",
        VALID_EFFECT,
        "- lvgl_fx.set: { id: fx_label, visible: maybe }",
        "Expected boolean value",
    ),
]


def run_case(case):
    name, fx, actions, expected = case
    actions = textwrap.indent(actions or "- delay: 1ms", "      ")
    text = BASE.format(components=COMPONENTS.as_posix(), fx=fx.rstrip("\n") or "  motion: {}", actions=actions)
    # `lvgl_fx:` with only a nested block needs the component id-less form; the base always has content.
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "case.yaml"
        path.write_text(text, encoding="utf-8")
        proc = subprocess.run(
            [sys.executable, "-m", "esphome", "config", str(path)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            env={**os.environ, "NO_COLOR": "1", "PYTHONIOENCODING": "utf-8"},
            check=False,
        )
    output = re.sub(r"\x1b\[[0-9;]*m", "", proc.stdout + proc.stderr)
    flat = re.sub(r"\s+", " ", output)
    if expected is None:
        ok = proc.returncode == 0
        detail = "" if ok else f"expected the config to pass:\n{output}"
    else:
        ok = proc.returncode != 0 and re.sub(r"\s+", " ", expected) in flat
        detail = "" if ok else f"expected failure containing {expected!r}, exit code {proc.returncode}:\n{output}"
    return name, expected, ok, detail


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-k", default="", help="only run the cases whose name contains this")
    parser.add_argument("-j", type=int, default=4, help="parallel esphome processes")
    args = parser.parse_args()
    cases = [case for case in CASES if args.k in case[0]]
    with ThreadPoolExecutor(max_workers=args.j) as pool:
        results = list(pool.map(run_case, cases))
    failed = 0
    for name, expected, ok, detail in results:
        print(f"{'PASS' if ok else 'FAIL'}  {name}" + (f"  ->  {expected}" if expected else ""))
        if not ok:
            failed += 1
            print(textwrap.indent(detail.strip(), "      "))
    print(f"\n{len(results) - failed}/{len(results)} cases as expected")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
