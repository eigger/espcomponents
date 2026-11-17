"""Exercise the real connection-sound state machine and GPS fix expression."""
from pathlib import Path
import json
import re
import shutil
import subprocess
import tempfile
import unittest
import wave
import yaml

ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = ROOT / "tab5.yaml"
if not CONFIG_PATH.exists():
    CONFIG_PATH = ROOT / "colorado-tab5-fallout.yaml"
CONFIG = yaml.load(CONFIG_PATH.read_text(), Loader=yaml.BaseLoader)
SCRIPTS = {s["id"]: s for s in CONFIG["script"]}


class SoundLinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            raise unittest.SkipTest("C++17 compiler required")
        init = next(g["initial_value"] for g in CONFIG["globals"] if g["id"] == "fn_sound_link")
        script = SCRIPTS["script_sound_links"]["then"][0]["lambda"]
        gps = script[script.index("auto &gps"):script.index("id(tab5_motion_visible)")]
        source = r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
struct Field {
  bool valid = true; uint32_t age_ms = 0; unsigned count = 4;
  bool isValid() { return valid; }
  uint32_t age() { return age_ms; }
  unsigned value() { return count; }
};
struct Tiny { Field location, satellites; };
struct GPS { Tiny tiny; Tiny &get_tiny_gps() { return tiny; } } id_gps;
#define id(x) (x)
'''
        source += "auto observe = " + init + ";\nbool fix() {" + gps + "return fix; }\n"
        source += r'''
int main(int argc, char **argv) {
  assert(argc == 2); std::string test = argv[1];
  if (test == "edges") {
    assert(observe(0, false, 0, true, true) == -1);
    assert(observe(0, false, 750, true, true) == -1); // Not a boot disconnection.
    assert(observe(0, true, 1000, true, true) == -1);
    assert(observe(0, true, 1749, true, true) == -1);
    assert(observe(0, true, 1750, true, true) == 9);
    assert(observe(0, true, 2000, true, true) == -1);
    assert(observe(0, false, 2500, true, true) == -1);
    assert(observe(0, false, 3250, true, true) == 10);
    assert(observe(0, false, 3500, true, true) == -1);
  } else if (test == "mute_ota") {
    for (unsigned link = 0; link < 3; ++link) {
      assert(observe(link, true, 0, false, true) == -1);
      assert(observe(link, true, 4000, false, true) == -1);
      assert(observe(link, true, 5000, true, true) == -1);
      assert(observe(link, false, 6000, false, true) == -1);
      assert(observe(link, false, 10000, false, true) == -1);
      assert(observe(link, false, 11000, true, true) == -1);
    }
  } else if (test == "boot_queue") {
    assert(observe(0, true, 0, true, false) == -1);
    assert(observe(1, true, 0, true, false) == -1);
    assert(observe(0, true, 750, true, false) == -1);
    assert(observe(1, true, 750, true, false) == -1);
    assert(observe(0, true, 3300, true, true) == 9);
    assert(observe(1, true, 3300, true, false) == -1);
    assert(observe(1, true, 3900, true, true) == 11);
  } else if (test == "queue") {
    assert(observe(1, true, 0, true, false) == -1);
    assert(observe(1, true, 750, true, false) == -1);
    assert(observe(1, true, 1000, true, true) == 11);
    assert(observe(1, false, 2000, true, false) == -1);
    assert(observe(1, false, 2750, true, false) == -1);
    assert(observe(1, true, 2800, true, true) == -1); // Cancel stale disconnect.
    assert(observe(1, true, 3550, true, true) == 11);
    assert(observe(1, false, 4000, true, false) == -1);
    assert(observe(1, false, 4750, true, false) == -1);
    assert(observe(1, false, 10000, true, true) == -1); // Expired queue.
  } else if (test == "gps") {
    assert(fix());
    id_gps.tiny.location.valid = false; assert(!fix());
    id_gps.tiny.location.valid = true;
    id_gps.tiny.location.age_ms = 3001; assert(!fix());
    id_gps.tiny.location.age_ms = 0;
    id_gps.tiny.satellites.age_ms = 3001; assert(!fix());
    id_gps.tiny.satellites.age_ms = 0;
    id_gps.tiny.satellites.count = 3; assert(!fix());
    assert(observe(2, false, 0, true, true) == -1);
    assert(observe(2, false, 3000, true, true) == -1);
    assert(observe(2, true, 4000, true, true) == -1);
    assert(observe(2, true, 6999, true, true) == -1);
    assert(observe(2, true, 7000, true, true) == 13);
    assert(observe(2, false, 8000, true, true) == -1);
    assert(observe(2, true, 9000, true, true) == -1);
    assert(observe(2, true, 12000, true, true) == -1); // Brief GPS loss suppressed.
    assert(observe(2, false, 13000, true, true) == -1);
    assert(observe(2, false, 16000, true, true) == 14);
  } else if (test == "wrap") {
    assert(observe(0, true, 0xFFFFFF00u, true, true) == -1);
    assert(observe(0, true, 493, true, true) == -1);
    assert(observe(0, true, 494, true, true) == 9);
    assert(observe(9, true, 500, true, true) == -1);
  } else return 2;
}
'''
        cls.temp = tempfile.TemporaryDirectory(prefix="colorado-sound-links-")
        cls.addClassCleanup(cls.temp.cleanup)
        path = Path(cls.temp.name)
        (path / "check.cpp").write_text(source)
        cls.binary = path / "check"
        result = subprocess.run([compiler, "-std=c++17", "-fsanitize=address,undefined",
                                 str(path / "check.cpp"), "-o", str(cls.binary)],
                                capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)

    def test_debounced_connect_disconnect_and_duplicate_suppression(self):
        subprocess.run([str(self.binary), "edges"], check=True)

    def test_disabled_events_are_not_replayed_after_mute_or_ota(self):
        subprocess.run([str(self.binary), "mute_ota"], check=True)

    def test_boot_connections_wait_and_play_instead_of_interrupting_boot(self):
        subprocess.run([str(self.binary), "boot_queue"], check=True)

    def test_busy_queue_cancels_stale_changes_and_expires(self):
        subprocess.run([str(self.binary), "queue"], check=True)

    def test_gps_requires_fresh_position_and_suppresses_brief_loss(self):
        subprocess.run([str(self.binary), "gps"], check=True)

    def test_millisecond_clock_wrap_and_invalid_link(self):
        subprocess.run([str(self.binary), "wrap"], check=True)

    def test_boot_starts_with_splash_and_clip_matches_animation_duration(self):
        actions = CONFIG["esphome"]["on_boot"][0]["then"]
        cue = next(i for i, a in enumerate(actions) if isinstance(a.get("script.execute"), dict)
                   and a["script.execute"].get("id") == "script_sound_play")
        animation = next(i for i, a in enumerate(actions) if "lvgl.animation.start" in a)
        hidden = next(i for i, a in enumerate(actions)
                      if a.get("lvgl.widget.hide", {}).get("id") == "obj_splash")
        self.assertLess(cue, animation)
        self.assertNotIn("execute(0)", actions[-1]["lambda"])
        self.assertIn("g_sound_ready) = true", actions[-1]["lambda"])
        duration = sum(int(a["delay"].removesuffix("ms"))
                       for a in actions[animation:hidden] if "delay" in a)
        manifest = json.loads((ROOT / "sounds/manifest.json").read_text())
        boot = next(r for r in manifest if r["file"] == "boot_mechanical_terminal.wav")
        self.assertEqual(duration, boot["duration_ms"])
        with wave.open(str(ROOT / "sounds" / boot["file"]), "rb") as stream:
            self.assertTrue(any(stream.readframes(stream.getnframes())[-3840:]),
                            "Boot audio must not have a silent final 40ms")
        play = SCRIPTS["script_sound_play"]["then"][0]["lambda"]
        self.assertIn("!id(g_sound_ready) && cue != 0", play)
        self.assertIn("id(g_sound_muted)", play)


if __name__ == "__main__":
    unittest.main()
