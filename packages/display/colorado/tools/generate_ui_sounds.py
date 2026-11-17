"""Generate the terminal UI sound preview set (48 kHz, mono, PCM16).

Run with Python 3; no third-party packages are required.
"""

from array import array
import math
from pathlib import Path
import random
import sys
import wave


RATE = 48000
OUT = Path(__file__).resolve().parents[1] / "sounds" / "archive" / "synthesized"
DURATIONS = dict(boot=1.1, menu_open=.22, menu_close=.22, click=.09,
                 success=.52, warning=.72, danger=.92, failure=.55, notice=.4)


def make_sound(name, duration):
    samples = [0.0] * round(duration * RATE)
    rng = random.Random(42)

    def tone(start, length, frequency, gain, end_frequency=None, bell=False):
        """Layer a gently driven FM voice with rounded attack and release."""
        phase = 0.0
        count = round(length * RATE)
        for j in range(count):
            i = round(start * RATE) + j
            if i >= len(samples):
                break
            t = j / RATE
            progress = j / max(1, count - 1)
            freq = frequency if end_frequency is None else (
                frequency * (end_frequency / frequency) ** progress)
            phase += math.tau * freq / RATE
            attack = min(1.0, t / .004)
            release = min(1.0, (length - t) / .025)
            decay = math.exp(-progress * (4.0 if bell else 1.8))
            fm = .55 * math.exp(-progress * 6) * math.sin(phase * 2)
            voice = (math.sin(phase + fm) + .22 * math.sin(phase * 2)
                     + .08 * math.sin(phase * 3))
            samples[i] += voice * gain * attack * release * decay

    def tick(start=0, gain=.1):
        # Short filtered noise transient adds a tactile edge without a sharp pop.
        low = 0.0
        for j in range(round(.014 * RATE)):
            i = round(start * RATE) + j
            if i >= len(samples):
                break
            low += .18 * (rng.uniform(-1, 1) - low)
            env = min(1, j / 24) * math.exp(-j / (RATE * .003))
            samples[i] += gain * low * env

    if name == "boot":
        tone(0, .36, 110, .3, 220)
        tone(.03, .34, 220, .16, 440)
        for start, freq in [(.20, 440), (.31, 554.37), (.42, 659.25), (.55, 880)]:
            tone(start, .40, freq, .23, bell=True)
        tone(.55, .49, 220, .15, bell=True)
        tone(.55, .49, 440, .13, bell=True)
        tick(0, .2)
    elif name in ("menu_open", "menu_close"):
        opening = name == "menu_open"
        tone(0, .16, 280 if opening else 560, .33, 560 if opening else 280)
        tone(.045, .15, 700 if opening else 420, .18, bell=True)
        tone(0, .12, 140, .15)
        tick(gain=.3)
    elif name == "click":
        tone(0, .065, 780, .34, 480)
        tone(0, .06, 195, .24)
        tick(gain=.5)
    elif name == "success":
        for start, freq in [(0, 523.25), (.095, 659.25), (.19, 783.99)]:
            tone(start, .27, freq, .30, bell=True)
        tone(.19, .27, 261.63, .18, bell=True)
        tone(.19, .27, 523.25, .10, bell=True)
    elif name == "warning":
        for start in [0, .32]:
            tone(start, .25, 587.33, .32, 554.37)
            tone(start, .25, 293.66, .18)
            tick(start, .14)
    elif name == "danger":
        for start in [0, .29, .58]:
            tone(start, .24, 880, .30, 659.25)
            tone(start, .24, 440, .22, 329.63)
            tone(start, .24, 220, .14)
            tick(start, .22)
    elif name == "failure":
        tone(0, .23, 493.88, .31, 440)
        tone(.17, .30, 349.23, .30, 246.94)
        tone(.17, .30, 174.61, .20, 123.47)
        tick(gain=.15)
    elif name == "notice":
        tone(0, .31, 659.25, .31, bell=True)
        tone(.09, .25, 987.77, .18, bell=True)
        tone(0, .31, 329.63, .14, bell=True)

    # A small early reflection gives body while keeping every cue inside its
    # original playback window. Use a dry copy to avoid uncontrolled feedback.
    dry = samples[:]
    for delay, gain in [(.019, .12), (.043, .07), (.071, .04)]:
        offset = round(delay * RATE)
        for i in range(offset, len(samples)):
            samples[i] += dry[i - offset] * gain

    # Remove DC and softly round peaks; preserve level differences between cues.
    mean = sum(samples) / len(samples)
    samples = [math.tanh((v - mean) * 1.4) for v in samples]
    peak = max(abs(v) for v in samples)
    rms = math.sqrt(sum(v * v for v in samples) / len(samples))
    target = .20 if name in ("warning", "danger") else .17
    gain = min(target / rms, .85 / peak)
    for i in range(len(samples)):
        fade = min(1.0, i / 96, (len(samples) - 1 - i) / 480)
        samples[i] *= gain * fade
    return samples


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, duration in DURATIONS.items():
        samples = make_sound(name, duration)
        pcm = array("h", (round(v * 32767) for v in samples))
        if sys.byteorder != "little":
            pcm.byteswap()
        with wave.open(str(OUT / f"{name}.wav"), "wb") as stream:
            stream.setparams((1, 2, RATE, len(samples), "NONE", "not compressed"))
            stream.writeframes(pcm.tobytes())
        rms = math.sqrt(sum(v * v for v in samples) / len(samples))
        assert max(abs(v) for v in samples) <= .85 + 1e-9
        assert samples[0] == samples[-1] == 0
        print(f"{name}: {duration:.2f}s, RMS {20 * math.log10(rms):.1f} dBFS")


if __name__ == "__main__":
    main()
