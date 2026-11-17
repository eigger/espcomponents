"""Make short connection cues from the approved terminal UI composites.

Requires NumPy and SciPy. Wi-Fi, OBD BLE and GPS use distinct pulse timing;
connection rises and disconnection falls. No new source recordings are used.
"""
from pathlib import Path
import json
import wave
import numpy as np
from scipy.signal import resample_poly

ROOT = Path(__file__).resolve().parents[1] / "sounds"
RATE = 48000


def read(name):
    with wave.open(str(ROOT / name), "rb") as w:
        assert (w.getframerate(), w.getnchannels(), w.getsampwidth()) == (RATE, 1, 2)
        return np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(float) / 32768


def main():
    sources = {True: read("ui_notice_terminal.wav"), False: read("ui_failure_terminal.wav")}
    report = {}
    for link, duration, positions in [("wifi", .46, [0, .18]),
                                      ("bluetooth", .38, [0, .10]),
                                      ("gps", .60, [0, .17, .34])]:
        for connected in [True, False]:
            out = np.zeros(round(duration * RATE))
            base = sources[connected][:round(.19 * RATE)]
            for n, start in enumerate(positions):
                ratio = (.94 + n * .12) if connected else (1.12 - n * .12)
                pulse = resample_poly(base, 100, round(ratio * 100))
                pulse[:96] *= np.linspace(0, 1, 96)
                pulse[-1440:] *= np.linspace(1, 0, 1440)
                offset = round(start * RATE)
                count = min(len(pulse), len(out) - offset)
                out[offset:offset + count] += pulse[:count]
            rms = np.sqrt(np.mean(out ** 2))
            out *= min(.09 / max(rms, 1e-9), .8 / max(np.max(np.abs(out)), 1e-9))
            out[:96] *= np.linspace(0, 1, 96)
            out[-960:] *= np.linspace(1, 0, 960)
            name = f"link_{link}_{'connected' if connected else 'disconnected'}.wav"
            pcm = np.rint(out * 32767).astype("<i2")
            assert pcm[0] == pcm[-1] == 0 and np.max(np.abs(out)) <= .8 + 1e-9
            with wave.open(str(ROOT / name), "wb") as w:
                w.setparams((1, 2, RATE, len(pcm), "NONE", "not compressed"))
                w.writeframes(pcm.tobytes())
            report[name] = dict(duration_ms=round(duration * 1000), pulse_positions_s=positions,
                                source="ui_notice_terminal.wav" if connected else "ui_failure_terminal.wav",
                                license="Pixabay Content License (adaptation; see ui_processing.json)")
            print(name, report[name]["duration_ms"], "ms")
    (ROOT / "connection_processing.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
