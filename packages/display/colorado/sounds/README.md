# Colorado UI sounds

Original synthesized product UI effects for the Tab5 dashboard, with layered resonances, filtered click transients, and short early reflections. No third-party recordings or downloaded samples are used.

All files are mono 48 kHz, signed 16-bit PCM WAV. They are intended to be downloaded at firmware build time and embedded with ESPHome's speaker media player's `files` option; playback is offline.

| File | Use | Duration |
| --- | --- | --- |
| boot.wav | Startup complete | 1100 ms |
| menu_open.wav | Open menu | 220 ms |
| menu_close.wav | Close menu | 220 ms |
| click.wav | Accepted button/tab selection | 90 ms |
| success.wav | Calibration complete / sound test | 520 ms |
| warning.wav | Caution | 720 ms |
| danger.wav | Critical warning | 920 ms |
| failure.wav | Failed operation | 550 ms |
| notice.wav | Informational notification | 400 ms |

`manifest.json` records SHA-256 hashes, duration, size and normalized peak amplitude. Each file has a faded attack/tail and zero-valued endpoints. Effects are level-balanced relative to one another; use the dashboard's master volume and mute controls for playback level. Check the actual Tab5 speaker at a low initial volume.
