"""Build the retro UI/link sound set by layering CC0 recordings with the alert chime.

Sources (all Freesound, CC0 1.0; HQ preview MP3s, not redistributed):
  switch   Breviceps   "Light Switch (on / off)"                457037
  tapedeck kyles       "cassette tape deck open, close +tape..."  450525
  blinker  MPierluissi "VEHCar_NISSAN SENTRA-TURNING SIGNAL_MP"   446322
  radio    RutgerMuller "Radio Noise.wav"                          50750
  dial     jc144940    "Radio Dial Tuning.aif"                    179118
  bell     _stubb      "Typewriter ding_near_mono.wav"            406243
plus the two published retro alert cues (door chime rgjpb 686265, relay buzzer
scruss 687948; both CC0). The chime motif ties every cue to the alert voice; the
recordings supply the varied mechanical/electrical character.

Also writes the alert cues with 120 ms of trailing digital silence, and a danger
alternative that strikes the caution chime twice.

Usage: python tools/generate_retro_ui.py SOUNDS_DIR SOURCES_DIR OUT_DIR
SOURCES_DIR holds the preview MP3s named as above (switch.mp3, ...).
Requires numpy, scipy and an ffmpeg executable (FFMPEG env var or on PATH).
"""
import json
import os
import subprocess
import sys
from fractions import Fraction
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, resample_poly, sosfiltfilt

FS = 48000
SOUNDS, SOURCES, OUT = (Path(p) for p in sys.argv[1:4])
OUT.mkdir(parents=True, exist_ok=True)
FFMPEG = os.environ.get('FFMPEG', 'ffmpeg')


def decode(path):
    raw = subprocess.run([FFMPEG, '-v', 'error', '-i', str(path), '-ac', '1', '-ar', str(FS),
                          '-f', 'f32le', 'pipe:1'], capture_output=True, check=True).stdout
    return np.frombuffer(raw, '<f4').astype(np.float64)


def wav(name):
    fs, x = wavfile.read(SOUNDS / name)
    assert fs == FS and x.dtype == np.int16 and x.ndim == 1, name
    return x.astype(np.float64) / 32768.0


def band(x, lo=None, hi=None):
    if lo:
        x = sosfiltfilt(butter(4, lo, 'highpass', fs=FS, output='sos'), x)
    if hi:
        x = sosfiltfilt(butter(4, hi, 'lowpass', fs=FS, output='sos'), x)
    return x


def fade(x, in_ms=1.0, out_ms=20.0):
    x = x.copy()
    a, r = min(len(x), int(FS * in_ms / 1000)), min(len(x), int(FS * out_ms / 1000))
    if a:
        x[:a] *= np.sin(np.linspace(0, np.pi / 2, a)) ** 2
    if r:
        x[len(x) - r:] *= np.cos(np.linspace(0, np.pi / 2, r)) ** 2
    return x


def peak1(x):
    return x / max(np.max(np.abs(x)), 1e-9)


def excerpt(src, at_s, length_ms, pre_ms=4, out_ms=None):
    """Cut around a located transient, starting pre_ms before it."""
    s = max(0, int(FS * (at_s - pre_ms / 1000)))
    x = src[s:s + int(FS * length_ms / 1000)]
    return peak1(fade(x, 1.0, out_ms if out_ms is not None else min(25, length_ms * 0.4)))


def strongest(src, near_s, window_ms=60):
    """Refine a coarse onset to the sample of maximum level nearby."""
    s = max(0, int(FS * (near_s - window_ms / 2000)))
    seg = np.abs(src[s:s + int(FS * window_ms / 1000)])
    return (s + int(np.argmax(seg))) / FS


def shift(x, semis):
    ratio = Fraction(2 ** (semis / 12)).limit_denominator(400)
    return resample_poly(x, ratio.denominator, ratio.numerator)


# ---- sources -------------------------------------------------------------
chime = wav('ui_warning_retro_chime.wav')
chime = chime[np.argmax(np.abs(chime) > 0.01) - 48:]
buzzer = wav('ui_danger_retro_buzzer.wav')
src = {k: band(decode(SOURCES / f'{k}.mp3'), 120) for k in
       ('switch', 'tapedeck', 'blinker', 'radio', 'dial', 'bell')}


def note(semis, length_ms, gain=1.0):
    x = shift(chime, semis)[: int(FS * length_ms / 1000)]
    return peak1(fade(x, 0.5, min(60, length_ms * 0.4))) * gain


def mix(parts, total_ms):
    out = np.zeros(int(FS * total_ms / 1000))
    for start_ms, sig in parts:
        s = int(FS * start_ms / 1000)
        e = min(len(out), s + len(sig))
        out[s:e] += sig[: e - s]
    return out


def finish(x, rms_db, tail_ms=80):
    x = band(x, 250, 10000)
    active = x[np.abs(x) > 1e-4]
    g = min(10 ** (rms_db / 20) / np.sqrt(np.mean(active ** 2)), 10 ** (-1 / 20) / np.max(np.abs(x)))
    x = fade(x * g, 0.5, 10)
    x[0] = 0.0
    return np.concatenate([x, np.zeros(int(FS * tail_ms / 1000))])


# Located transients (seconds) in each preview.
SW_ON, SW_OFF = strongest(src['switch'], 0.05), strongest(src['switch'], 0.40)
DECK_A, DECK_B = strongest(src['tapedeck'], 0.27), strongest(src['tapedeck'], 9.34)
TICK, TOCK = strongest(src['blinker'], 0.45), strongest(src['blinker'], 0.80)
BELL = strongest(src['bell'], 0.05)

switch_on = excerpt(src['switch'], SW_ON, 70)
switch_off = excerpt(src['switch'], SW_OFF, 70)
deck_a = excerpt(src['tapedeck'], DECK_A, 160)
deck_b = excerpt(src['tapedeck'], DECK_B, 160)
tick = excerpt(src['blinker'], TICK, 60)
tock = excerpt(src['blinker'], TOCK, 60)
bell = excerpt(src['bell'], BELL, 700, out_ms=250)
static = peak1(fade(band(src['radio'][int(4.20 * FS):int(4.20 * FS) + int(0.22 * FS)], 700, 5000), 15, 90))
tuning = peak1(fade(band(src['dial'][int(1.60 * FS):int(1.60 * FS) + int(0.30 * FS)], 500, 6000), 20, 120))
relay = peak1(fade(buzzer[np.argmax(np.abs(buzzer) > 0.05):][:int(0.12 * FS)], 1, 30))

UI = -21.0  # current UI/link cues -19..-22 dB RMS; alerts -18 / -14 stay louder
cues = {
    'ui_click_retro.wav': (mix([(0, switch_on), (2, note(+24, 40, 0.18))], 70), -22.5,
                           'Toggle-switch snap with a faint chime glint'),
    'ui_menu_open_retro.wav': (mix([(0, deck_a * 0.9), (60, note(+5, 140, 0.55)), (125, note(+10, 230, 0.7))], 360), UI,
                               'Tape-deck latch, then rising chime fourth'),
    'ui_menu_close_retro.wav': (mix([(0, note(+10, 140, 0.55)), (65, note(+5, 200, 0.65)), (150, deck_b * 0.9)], 320), UI,
                                'Falling chime fourth closing on a tape-deck clunk'),
    'ui_success_retro.wav': (mix([(0, switch_off * 0.7), (45, bell), (120, note(+12, 300, 0.35))], 650), UI,
                             'Switch throw and typewriter bell, chime octave under it'),
    'ui_failure_retro.wav': (mix([(0, tock * 0.9), (40, note(+1, 150, 0.6)), (140, note(0, 180, 0.6)),
                                  (300, relay * 0.6)], 440), UI,
                             'Relay clack, falling semitone, short relay buzz'),
    'ui_notice_retro.wav': (mix([(0, shift(bell, -5)[: int(FS * 0.32)] * 0.8), (200, shift(bell, -5)[: int(FS * 0.32)] * 0.55)], 520),
                            UI - 1.5, 'Two soft lowered typewriter bells'),
    # Chime-only replacements: the typewriter bell stood out from the chime voice.
    'ui_success_retro_chime.wav': (mix([(0, note(+4, 120, 0.55)), (70, note(+7, 120, 0.6)),
                                        (140, note(+12, 420, 0.75))], 570), UI,
                                   'Quick rising chime major arpeggio'),
    'ui_notice_retro_chime.wav': (mix([(0, note(+12, 520, 0.8))], 520), UI - 1.5,
                                  'One light chime an octave up'),
    # Dull "tuk": a 50 ms chime strike a fifth up with the ring low-passed off.
    'ui_click_retro_chime.wav': (mix([(0, peak1(fade(band(note(+7, 50), hi=3500), 0.5, 30)))], 70), -22.5,
                                 'Short low-passed chime strike'),
}
# Links. Wi-Fi: radio static; Bluetooth: turn-signal relay tick-tock; GPS: dial tuning sweep.
# Connected: texture resolves into a rising chime fifth. Disconnected: falling fifth dissolving into it.
link_layers = {'wifi': (+7, static), 'bluetooth': (0, None), 'gps': (-5, tuning)}
for link, (base, texture) in link_layers.items():
    if link == 'bluetooth':
        lead = mix([(0, tick), (95, tock * 0.8)], 160)
        tail = mix([(0, tock * 0.8), (95, tick * 0.6)], 160)
    else:
        lead, tail = texture[: int(FS * 0.16)], texture[::-1][: int(FS * 0.18)] * 0.8
        lead, tail = fade(lead, 15, 40), fade(tail, 20, 60)
    up = mix([(0, lead * 0.7), (110, note(base, 150, 0.6)), (200, note(base + 7, 260, 0.75))], 460)
    down = mix([(0, note(base + 7, 150, 0.6)), (90, note(base, 220, 0.7)), (230, tail * 0.6)], 440)
    cues[f'link_{link}_connected_retro.wav'] = (up, UI, f'{link}: texture then rising fifth')
    cues[f'link_{link}_disconnected_retro.wav'] = (down, UI, f'{link}: falling fifth into texture')

manifest = []
for name, (signal, rms, desc) in cues.items():
    y = finish(signal, rms)
    pcm = np.round(np.clip(y, -1, 1) * 32767).astype(np.int16)
    wavfile.write(OUT / name, FS, pcm)
    manifest.append({'file': name, 'duration_ms': round(len(pcm) * 1000 / FS), 'description': desc})

for s, d in (('ui_warning_retro_chime.wav', 'ui_warning_retro_chime_padded.wav'),
             ('ui_danger_retro_buzzer.wav', 'ui_danger_retro_buzzer_padded.wav')):
    fs, x = wavfile.read(SOUNDS / s)
    y = np.concatenate([x, np.zeros(int(FS * 0.12), np.int16)])
    wavfile.write(OUT / d, fs, y)
    manifest.append({'file': d, 'duration_ms': round(len(y) * 1000 / FS), 'description': f'{s} + 120 ms silence'})

# Danger alternative: the caution chime struck twice, 220 ms apart (the second strike rings
# over the first one's decay), leveled to the buzzer's active RMS so danger stays louder.
def active_rms(x):
    w = np.array([np.sqrt(np.mean(b ** 2)) for b in np.array_split(x, max(1, len(x) // 480))])
    a = np.flatnonzero(w > w.max() * 0.05)
    return np.sqrt(np.mean(x[a[0] * 480:(a[-1] + 1) * 480] ** 2))


chime = wavfile.read(SOUNDS / 'ui_warning_retro_chime.wav')[1] / 32767.0
buzz = wavfile.read(SOUNDS / 'ui_danger_retro_buzzer.wav')[1] / 32767.0
double = np.zeros(int(FS * 0.22) + len(chime))
for at in (0, int(FS * 0.22)):
    double[at:at + len(chime)] += chime
double *= active_rms(buzz) / active_rms(double)
double *= min(1.0, 0.89 / np.max(np.abs(double)))
double = np.round(double * 32767).astype(np.int16)
for name, pad in (('ui_danger_retro_chime_double.wav', 0), ('ui_danger_retro_chime_double_padded.wav', 0.12)):
    y = np.concatenate([double, np.zeros(int(FS * pad), np.int16)])
    wavfile.write(OUT / name, FS, y)
    manifest.append({'file': name, 'duration_ms': round(len(y) * 1000 / FS),
                     'description': 'ui_warning_retro_chime.wav struck twice, 220 ms apart'
                                    + (' + 120 ms silence' if pad else '')})

(OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
for m in manifest:
    print(f"{m['file']:40} {m['duration_ms']:5} ms  {m['description']}")
