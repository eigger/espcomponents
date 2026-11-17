"""Build three boot previews from the user's selected Pixabay recordings.

Usage: python process_boot_candidates.py /path/to/source-mp3-directory
Requires numpy, scipy, imageio-ffmpeg. Original recordings stay outside the repo.
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
FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()
SOURCES = {
    "tape": ("Tape Deck Startup", "soundjoao / freesound_community", "film-special-effects-tape-deck-startup-103423"),
    "hdd": ("043876_HDD Startup & Shutdown", "Freesound Community", "film-special-effects-043876-hdd-startup-amp-shutdown-79401"),
    "turbine": ("Turbine", "EchoCinematics / freesound_community", "film-special-effects-turbine-77394"),
    "eightbit": ("8Bit Computer Startup", "Janosch-JR / freesound_community", "technology-8bit-computer-startup-66972"),
    "engine": ("motor de coche arrancando", "nokono (AI generated)", "film-special-effects-motor-de-coche-arrancando-406653"),
}

# source, source start, source duration, tempo, placement, relative gain
RECIPES = {
    "boot_mechanical_terminal": (3.3, [
        ("tape", .02, 2.55, 1, 0, .85),
        ("hdd", .12, 3.6, 1.2, .28, .65),
    ]),
    "boot_turbine_terminal": (3.6, [
        ("tape", .41, .19, 1, 0, .55),
        ("turbine", .1, 9.0, 3, .08, .85),
        ("eightbit", .12, .62, 1, 2.87, .5),
    ]),
    "boot_engine_terminal": (3.2, [
        ("engine", 2.9, 2.9, 1, .02, 1),
        ("tape", .41, .19, 1, 0, .25),
        ("eightbit", .12, .43, 1, 2.65, .32),
    ]),
}


def extract(source_dir, name, start, duration, tempo):
    command = [FFMPEG, "-v", "error", "-ss", str(start), "-t", str(duration),
               "-i", str(source_dir / f"{name}.mp3"), "-vn", "-ac", "1",
               "-af", f"atempo={tempo}", "-ar", str(RATE), "-f", "f32le", "pipe:1"]
    result = subprocess.run(command, capture_output=True, check=True)
    x = np.frombuffer(result.stdout, dtype="<f4").astype(np.float64)
    assert len(x), name
    x -= np.mean(x)
    x = sosfilt(butter(2, [95, 6500], btype="bandpass", fs=RATE, output="sos"), x)
    # Control transients before matching the quieter mechanical textures.
    scale = max(float(np.percentile(np.abs(x), 99.7)), .0001)
    x = .65 * np.tanh(x / scale * .8)
    fade_in = min(round(.004 * RATE), len(x))
    fade_out = min(round(.09 * RATE), len(x))
    x[:fade_in] *= np.linspace(0, 1, fade_in)
    x[-fade_out:] *= np.linspace(1, 0, fade_out)
    return x


def main():
    source_dir = Path(sys.argv[1])
    OUT.mkdir(parents=True, exist_ok=True)
    report = {}
    for name, (duration, layers) in RECIPES.items():
        output = np.zeros(round(duration * RATE))
        for source, start, length, tempo, at, level in layers:
            x = extract(source_dir, source, start, length, tempo)
            pos = round(at * RATE)
            length = min(len(x), len(output) - pos)
            output[pos:pos + length] += level * x[:length]
        # A short room reflection adds body while remaining mono-compatible.
        dry = output.copy()
        for seconds, level in [(.023, .08), (.051, .035)]:
            delay = round(seconds * RATE)
            output[delay:] += dry[:-delay] * level
        output = np.tanh(output)
        rms = np.sqrt(np.mean(output ** 2))
        output *= min(.14 / max(rms, 1e-8), .89 / max(np.max(np.abs(output)), 1e-8))
        output[:240] *= np.linspace(0, 1, 240)
        output[-7200:] *= np.linspace(1, 0, 7200)
        pcm = np.rint(output * 32767).astype("<i2")
        assert np.isfinite(output).all()
        assert np.max(np.abs(output)) <= .89 + 1e-8
        assert pcm[0] == pcm[-1] == 0
        with wave.open(str(OUT / f"{name}.wav"), "wb") as stream:
            stream.setparams((1, 2, RATE, len(pcm), "NONE", "not compressed"))
            stream.writeframes(pcm.tobytes())
        report[name] = dict(duration_s=duration,
                            peak_dbfs=round(float(20 * np.log10(np.max(np.abs(output)))), 2),
                            rms_dbfs=round(float(20 * np.log10(np.sqrt(np.mean(output ** 2)))), 2),
                            layers=layers)
        print(name, report[name])
    (OUT / "boot_processing.json").write_text(json.dumps(report, indent=2) + "\n")
    credits = ["# Boot sound sources and processing", "", "Composite boot sounds. The selected default is boot_mechanical_terminal.wav.",
               "48 kHz, mono, PCM16. Edits: excerpts, pitch-preserving time compression,",
               "layering, speaker-oriented EQ, transient control, short reflection and fades.", "",
               "Sources selected by the user (accessed 2026-11-17):", ""]
    for title, author, slug in SOURCES.values():
        credits.append(f"- [{title}](https://pixabay.com/sound-effects/{slug}/) — {author}")
    credits += ["", "License: [Pixabay Content License](https://pixabay.com/service/license-summary/).",
                "Original MP3s are not included. These samples are not relicensed as project code.",
                "Rebuild with tools/process_boot_candidates.py and your local source MP3s.", ""]
    (OUT / "BOOT_SOURCES.md").write_text("\n".join(credits))


if __name__ == "__main__":
    main()
