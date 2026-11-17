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
| `boot_engine_terminal.wav` | Engine starting + terminal details | 3200 ms |
| `boot.wav` | Original synthesized boot; retained for existing configurations | 1100 ms |

The composite recordings are adaptations of the user-selected Pixabay sources.
See [BOOT_SOURCES.md](BOOT_SOURCES.md) for authors and license links, and
[boot_processing.json](boot_processing.json) for excerpts, timing and processing.
Original downloaded recordings are not included.

## Existing UI sounds

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

These existing files and the archive are original synthesized sounds. The UI
defaults above are retained while the [replacement shortlist](UI_CANDIDATES.md)
is evaluated.

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
```

The composite tool expects `tape.mp3`, `hdd.mp3`, `turbine.mp3`, `eightbit.mp3`,
and `engine.mp3` downloaded from the linked source pages. It requires NumPy,
SciPy, and imageio-ffmpeg. The synthesized UI generator uses Python's standard
library. The two standalone archived boot sketches are retained WAV renders.

When selecting a different boot file, also update the dashboard's cue 0 duration
to the WAV duration in milliseconds so another UI cue cannot cut it short.
