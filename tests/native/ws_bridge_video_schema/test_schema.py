"""Config-validation regression tests for ws_bridge snapshot video options.

Run: python tests/native/ws_bridge_video_schema/test_schema.py
Requires the esphome Python package.
"""
import importlib
import sys
from pathlib import Path

import esphome.config_validation as cv

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "components"))
ws = importlib.import_module("ws_bridge")

BASE = {"camera_id": "cam", "unique_id": "front", "name": "Front"}


def accepts(**extra):
    try:
        ws.SNAPSHOT_SCHEMA({**BASE, **extra})
        return True
    except cv.Invalid:
        return False


def main():
    assert accepts(), "plain still snapshot"
    assert accepts(video=True), "video with defaults"
    assert accepts(video=True, video_fps=30, video_max_bandwidth=1)
    assert not accepts(video=True, video_max_bandwidth=0), "bandwidth 0 divides by zero"
    assert not accepts(video=True, video_fps=0)
    assert not accepts(video=True, video_fps=31)
    assert not accepts(video=True, platform="image"), "image cannot stream"
    assert not accepts(video_fps=5), "video_fps without video"
    assert not accepts(video_max_bandwidth=1000), "bandwidth without video"
    print("ws_bridge video schema: OK")


if __name__ == "__main__":
    main()
