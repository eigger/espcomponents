# Colorado Tab5 sound library

Selected boot sound: **[`boot_mechanical_terminal.wav`](boot_mechanical_terminal.wav)**,
the tape-deck + HDD composite (candidate 1), **3300 ms**.

All WAVs are mono, 48 kHz, signed 16-bit PCM. ESPHome embeds only files explicitly
listed in `media_player.files`; the alternatives and archive do not consume flash
unless selected. Use a pinned Git commit in the raw asset URL.

## Boot sounds

| File | Description | Duration |
| --- | --- | --- |
| `boot_mechanical_terminal.wav` | Selected: tape deck + HDD | 3300 ms |
| `boot_turbine_terminal.wav` | Turbine spool-up + terminal details | 3600 ms |
| `boot_engine_terminal.wav` | Fixed engine-start boot cue + terminal details; not RPM-driven exhaust | 3200 ms |
| `boot.wav` | Original synthesized boot; retained for existing configurations | 1100 ms |

The composite recordings are adaptations of the user-selected Pixabay sources.
See [BOOT_SOURCES.md](BOOT_SOURCES.md) for authors and license links, and
[boot_processing.json](boot_processing.json) for excerpts, timing and processing.
Original downloaded recordings are not included.

## Retro warning and danger sounds

| File | Event | Duration |
| --- | --- | --- |
| [`ui_warning_retro_chime.wav`](ui_warning_retro_chime.wav) | Caution: single vintage car chime | 750 ms |
| [`ui_danger_retro_buzzer.wav`](ui_danger_retro_buzzer.wav) | Danger: two short buzzes and one long buzz | 1350 ms |
| [`ui_danger_retro_chime_double.wav`](ui_danger_retro_chime_double.wav) | Danger (alternative): the caution chime struck twice, 220 ms apart | 970 ms |

The caution cue adapts [rgjpb's Ford/GM-style door chime recreation](https://freesound.org/people/rgjpb/sounds/686265/).
The danger cue uses [scruss's recording of a 1940s Signal Electric R-68 Morse practice relay buzzer](https://freesound.org/people/scruss/sounds/687948/).
Both sources are [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
The chime is trimmed with a short fade. The recorded buzzer is slowed to
quarter speed to bring its high-frequency buzz into the speaker's midrange,
then cut into three pulses. Both cues are band-limited and level-adjusted.

The terminal warning and danger files below are still available with the same
durations. The two pairs can be exchanged by changing the filenames in the
dashboard's `media_player.files`.

`ui_warning_retro_chime_padded.wav` (870 ms) and `ui_danger_retro_buzzer_padded.wav`
(1470 ms) are the same audio followed by 120 ms of digital silence, so a speaker stop that
drops its final buffer can only cut silence. The unpadded files remain for older pins.
`ui_danger_retro_chime_double_padded.wav` (1090 ms) pads the double-chime danger cue the
same way. It reuses the caution chime, so caution and danger share one voice; it is leveled
to the buzzer's loudness and built by [`tools/generate_retro_ui.py`](../tools/generate_retro_ui.py).

## Retro UI and connection sounds

Built by [`tools/generate_retro_ui.py`](../tools/generate_retro_ui.py) by layering CC0
recordings with the retro caution chime, so menus, clicks and connection cues share the
alert voice while each event keeps its own mechanical/electrical character.
See [`retro_ui_spectrograms.png`](retro_ui_spectrograms.png).

| File | Event | Duration | Layers |
| --- | --- | --- | --- |
| `ui_click_retro.wav` | Button | 150 ms | toggle-switch snap, faint chime glint |
| `ui_click_retro_chime.wav` | Button (chime only) | 150 ms | short low-passed chime strike |
| `ui_menu_open_retro.wav` | Menu open | 440 ms | tape-deck latch, rising chime fourth |
| `ui_menu_close_retro.wav` | Menu close | 400 ms | falling chime fourth, tape-deck clunk |
| `ui_success_retro.wav` | Success | 730 ms | switch throw, typewriter bell, chime octave |
| `ui_success_retro_chime.wav` | Success (chime only) | 650 ms | quick rising chime major arpeggio |
| `ui_failure_retro.wav` | Failure | 520 ms | relay clack, falling semitone, short relay buzz |
| `ui_notice_retro.wav` | Information | 600 ms | two soft lowered typewriter bells |
| `ui_notice_retro_chime.wav` | Information (chime only) | 600 ms | one light chime an octave up |
| `link_wifi_{connected,disconnected}_retro.wav` | Wi-Fi | 540 / 520 ms | radio static, high fifth |
| `link_bluetooth_{connected,disconnected}_retro.wav` | Bluetooth | 540 / 520 ms | turn-signal relay tick-tock, middle fifth |
| `link_gps_{connected,disconnected}_retro.wav` | GPS | 540 / 520 ms | radio-dial tuning sweep, low fifth |

Connected cues rise a fifth after the texture; disconnected cues fall a fifth into it.
All cues are leveled near −21 dB RMS (alerts stay louder), with a zero first sample and at
least 80 ms of trailing silence.

Sources, all [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) (Freesound previews,
not redistributed): [Light Switch (on / off)](https://freesound.org/people/Breviceps/sounds/457037/) by Breviceps,
[cassette tape deck open, close + tape handling](https://freesound.org/people/kyles/sounds/450525/) by kyles,
[Nissan Sentra turning signal](https://freesound.org/people/MPierluissi/sounds/446322/) by MPierluissi,
[Radio Noise](https://freesound.org/people/RutgerMuller/sounds/50750/) by RutgerMuller,
[Radio Dial Tuning](https://freesound.org/people/jc144940/sounds/179118/) by jc144940,
[Typewriter ding](https://freesound.org/people/_stubb/sounds/406243/) by _stubb,
plus the door chime and relay buzzer credited above.

## Terminal UI sounds

The terminal set adapts the approved RescopicSound Modern Interface recordings,
with trimmed tails, pitch-preserving tempo adjustment, speaker-oriented EQ and
a quiet mechanical transient from the selected boot composite. Caution uses one
alarm pulse; danger uses two. See [ui_processing.json](ui_processing.json) for
source links, authors, excerpts and processing. The applicable license is the
[Pixabay Content License](https://pixabay.com/service/license-summary/).

| File | Event | Duration |
| --- | --- | --- |
| `ui_menu_open_terminal.wav` | Menu open | 650 ms |
| `ui_menu_close_terminal.wav` | Menu close | 600 ms |
| `ui_click_terminal.wav` | Button selection | 180 ms |
| `ui_success_terminal.wav` | Success | 650 ms |
| `ui_warning_terminal.wav` | Caution: single pulse | 750 ms |
| `ui_danger_terminal.wav` | Danger: double pulse | 1350 ms |
| `ui_failure_terminal.wav` | Failed operation | 420 ms |
| `ui_notice_terminal.wav` | Information | 550 ms |

## Legacy UI sounds

| File | Event | Duration |
| --- | --- | --- |
| `menu_open.wav` | Open menu | 220 ms |
| `menu_close.wav` | Close menu | 220 ms |
| `click.wav` | Button/tab selection | 90 ms |
| `success.wav` | Completed operation / sound test | 520 ms |
| `warning.wav` | Caution | 720 ms |
| `danger.wav` | Critical warning | 920 ms |
| `failure.wav` | Failed operation | 550 ms |
| `notice.wav` | Information | 400 ms |

These legacy files and the archive are original synthesized sounds, preserved
for older pinned configurations. The terminal and retro alternatives are listed above.

## Archived experiments

`archive/synthesized/` contains all eleven earlier synthesized previews: a nine-cue
UI set plus `boot_engine_synth.wav` and `boot_terminal_synth.wav`. They remain
available for comparison and are not selected by the dashboard.

`manifest.json` records the format, duration, peak, size and SHA-256 of every WAV.
Third-party-derived recordings keep the license listed for their source in the
manifest.

## Rebuilding

From the Colorado directory, run:

```sh
python tools/process_boot_candidates.py /path/to/original-mp3s
python tools/generate_ui_sounds.py
python tools/process_ui_sounds.py /path/to/preview-mp3s
```

The composite tool expects `tape.mp3`, `hdd.mp3`, `turbine.mp3`, `eightbit.mp3`,
and `engine.mp3` downloaded from the linked source pages. It requires NumPy,
SciPy, and imageio-ffmpeg. The synthesized UI generator uses Python's standard
library. The two standalone archived boot sketches are retained WAV renders.

When selecting a different boot file, also update the dashboard's cue 0 duration
to the WAV duration in milliseconds so another UI cue cannot cut it short.

## Connection notifications

Wi-Fi, OBD BLE and GPS have distinct connected/disconnected pairs:
`link_wifi_connected.wav` / `link_wifi_disconnected.wav` (460 ms),
`link_bluetooth_connected.wav` / `link_bluetooth_disconnected.wav` (380 ms), and
`link_gps_connected.wav` / `link_gps_disconnected.wav` (600 ms).
These are rhythmic/pitched adaptations of the approved notice/failure composites;
see `connection_processing.json` and `ui_processing.json` for provenance.
Run `python tools/generate_connection_sounds.py` to reproduce them.

Connections are debounced for 750 ms (Wi-Fi/OBD BLE) and 3 seconds (GPS).
GPS requires a valid position and at least four satellites with data no older
than 3 seconds; stale UART data therefore also triggers fix loss. Brief GPS loss
is suppressed. Initial disconnected states are silent. Connections acquired during boot wait until the dashboard is revealed.
Changes during mute or OTA do not replay later. A short queue lets simultaneous changes finish
without interrupting another effect; obsolete events are cancelled and queued
notifications expire after 5 seconds. Vehicle warnings retain higher priority.

The selected boot cue starts with the splash, before ordinary sound events are
enabled. The visual sequence waits at most one second for audio startup and then
runs for the boot WAV's 3300 ms duration. Normal sounds are enabled only after
the dashboard is revealed. The visual sequence continues when sound is muted.
