"""Build dashboard UI composites from the approved RescopicSound previews.

Usage: python tools/process_ui_sounds.py /path/to/preview-mp3-directory
Requires numpy, scipy and imageio-ffmpeg. Source MP3s are not published.
"""

from pathlib import Path
import json
import subprocess
import sys
import wave

import imageio_ffmpeg
import numpy as np
from scipy.signal import butter, sosfilt

RATE = 48000
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "sounds"
SLUGS = {
    "click": "ui-click-menu-modern-interface-select-small-01-230473",
    "menu_open": "ui-movement-menu-modern-interface-window-open-small-230486",
    "menu_close": "ui-movement-menu-modern-interface-window-close-small-230483",
    "success": "ui-alert-menu-modern-interface-confirm-small-230482",
    "failure": "ui-alert-menu-modern-interface-deny-small-230476",
    "notice": "ui-alert-menu-modern-interface-pop-up-small-230480",
    "warning": "ui-alert-alarm-modern-interface-warning-01-230492",
}
DURATIONS = dict(menu_open=.65, menu_close=.60, click=.18, success=.65,
                 warning=.75, danger=1.35, failure=.42, notice=.55)


def decode(path, filters=None):
    command = [imageio_ffmpeg.get_ffmpeg_exe(), "-v", "error", "-i", str(path)]
    if filters:
        command += ["-af", filters]
    command += ["-ac", "1", "-ar", str(RATE), "-f", "f32le", "pipe:1"]
    return np.frombuffer(subprocess.run(command, capture_output=True, check=True).stdout,
                         dtype="<f4").astype(np.float64)


def fades(x, attack=.003, release=.035):
    a, r = min(len(x), round(RATE * attack)), min(len(x), round(RATE * release))
    x[:a] *= np.linspace(0, 1, a)
    x[-r:] *= np.linspace(1, 0, r)
    return x


def main():
    source_dir = Path(sys.argv[1])
    mechanical = decode(OUT / "boot_mechanical_terminal.wav")[:round(.7 * RATE)]
    peak = int(np.argmax(np.abs(mechanical)))
    tactile = mechanical[max(0, peak - 144):peak + 1440].copy()
    tactile = fades(tactile, .001, .012)
    tactile *= .012 / max(np.sqrt(np.mean(tactile ** 2)), 1e-8)
    report = {}
    for event, duration in DURATIONS.items():
        source = "warning" if event == "danger" else event
        path = source_dir / f"{source}.mp3"
        x = decode(path)
        if source == "warning":
            start, end = 0, .80  # One complete pulse from the original alarm.
            target = .60 if event == "danger" else .70
        else:
            # Trim silence and very quiet reverb using 10ms RMS windows.
            windows = [np.sqrt(np.mean(b ** 2)) for b in np.array_split(x, max(1, len(x) // 480))]
            active = np.flatnonzero(np.array(windows) > max(windows) * .07)
            start = max(0, (active[0] - 1) * .01)
            end = min(len(x) / RATE, (active[-1] + 3) * .01)
            target = duration - .02
        tempo = (end - start) / target
        filters = f"atrim=start={start}:end={end},asetpts=PTS-STARTPTS,atempo={tempo}"
        voice = decode(path, filters)
        voice = sosfilt(butter(2, [110, 7000], btype="bandpass", fs=RATE, output="sos"), voice)
        voice -= np.mean(voice)
        voice *= .125 / max(np.sqrt(np.mean(voice ** 2)), 1e-8)
        voice = fades(voice, .003, .055)
        output = np.zeros(round(duration * RATE))
        starts = [0, .68] if event == "danger" else [0]
        for at in starts:
            offset = round(at * RATE)
            count = min(len(voice), len(output) - offset)
            output[offset:offset + count] += voice[:count]
            count = min(len(tactile), len(output) - offset)
            output[offset:offset + count] += tactile[:count]
        # The tape/HDD transient links these custom composites to the boot sound.
        output = fades(np.tanh(output), .002, .035)
        output *= min(1, .89 / max(np.max(np.abs(output)), 1e-8))
        pcm = np.rint(output * 32767).astype("<i2")
        assert np.isfinite(output).all() and pcm[0] == pcm[-1] == 0
        name = f"ui_{event}_terminal.wav"
        with wave.open(str(OUT / name), "wb") as stream:
            stream.setparams((1, 2, RATE, len(pcm), "NONE", "not compressed"))
            stream.writeframes(pcm.tobytes())
        report[name] = dict(duration_ms=round(duration * 1000), source=source,
                            source_start_s=start, source_end_s=end, tempo=tempo,
                            pulse_positions_s=starts,
                            mechanical_layer="boot_mechanical_terminal.wav transient",
                            author="RescopicSound", source_url="https://pixabay.com/sound-effects/film-special-effects-" + SLUGS[source] + "/")
        print(name, report[name]["duration_ms"], "ms")
    (OUT / "ui_processing.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
