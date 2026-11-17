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

## Active UI sounds

The active set adapts the approved RescopicSound Modern Interface recordings,
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
for older pinned configurations. The new active set is listed above.

## Archived experiments

`archive/synthesized/` contains all eleven earlier synthesized previews: a nine-cue
UI set plus `boot_engine_synth.wav` and `boot_terminal_synth.wav`. They remain
available for comparison and are not selected by the dashboard.

`manifest.json` records the format, duration, peak, size and SHA-256 of every WAV.
Third-party-derived recordings retain their applicable Pixabay terms and are not
relicensed under the repository's software license.

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
