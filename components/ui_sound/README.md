# ui_sound

[Installation](#installation) · [Quick start](#quick-start) · [Configuration](#configuration) ·
[Actions](#actions) · [C++ API](#c-api-for-lambdas) · [Policy](#policy) · [Caveats](#caveats)

Short UI cue sounds (clicks, success, warnings) through an ESPHome
[speaker media player](https://esphome.io/components/media_player/speaker/), with the policy every UI-sound device
needs: cue priority, no backlog, per-cue cooldown, a global minimum gap, and an optional drop of cues while the player is muted.

It is general purpose and does not depend on LVGL (or any display): call it from any automation. Tested with ESPHome
2026.11.0 on ESP32 (ESP-IDF).

## Installation

```yaml
external_components:
  - source: github://eigger/espcomponents@master
    components: [ ui_sound ]
```

`ui_sound` needs a `media_player` with `platform: speaker`; the sounds are the player's `files:` (compiled into
flash). Until the next release tag contains this component, use `@master` rather than `@latest`.

## Quick start

```yaml
media_player:
  - platform: speaker
    id: ui_sound_player
    announcement_pipeline:
      speaker: my_speaker
      format: WAV
      sample_rate: 16000
      num_channels: 1
    files:
      - id: sound_click
        file: click.wav
      - id: sound_error
        file: error.wav

ui_sound:
  media_player: ui_sound_player
  cues:
    - id: click
      file: sound_click
    - id: error
      file: sound_error
      priority: 10

# anywhere an automation runs, for example an LVGL button:
lvgl:
  pages:
    - widgets:
        - button:
            on_click:
              - ui_sound.play: click
```

## Configuration

| Option | Default | Meaning |
| --- | --- | --- |
| `id` | generated | Component id (needed for `id(...)` in lambdas or with several instances) |
| `media_player` | required | A `speaker` platform media player |
| `announcement` | `true` | Play cues as announcements (they duck/pause media playing on the same player) |
| `min_gap` | `0ms` | Minimum time between the starts of any two cues |
| `start_grace` | `300ms` | Busy window after a play request, while the player state catches up |
| `max_busy` | `10s` | Safety cap: a cue is never considered playing longer than this |
| `require_start` | `false` | `true`: cues are dropped until `ui_sound.start` (boot sequences) |
| `skip_when_muted` | `false` | `true`: drop cues while the player is muted or at volume 0 (see [Caveats](#caveats)) |
| `cues` | required | List of cues (at least one) |

Each cue:

| Option | Default | Meaning |
| --- | --- | --- |
| `id` | required | Cue id, used by `ui_sound.play`; a mistyped id is a configuration error |
| `file` | required | Media file id from the media player's `files:` |
| `priority` | `1` | 0-255; a higher priority interrupts a lower one |
| `cooldown` | `0ms` | Minimum time between two starts of this cue |
| `duration` | `0ms` | Optional: the cue counts as playing at least this long (files with trailing silence) |

## Actions

```yaml
- ui_sound.play: click                  # short form
- ui_sound.play:
    id: ui_sounds                       # optional with a single component
    cue: error
    force: !lambda "return x > 3;"      # templatable, default false
- ui_sound.stop:                        # stop the current cue, clear the busy state
- ui_sound.start:                       # declare the UI ready (see require_start)
- ui_sound.set_enabled: false           # bool or lambda; dropped cues are not queued
```

Condition: `ui_sound.is_playing` is true while a cue is considered playing.

## C++ API (for lambdas)

```cpp
auto r = id(ui_sounds).play(id(click));          // ui_sound::PlayResult
auto f = id(ui_sounds).play(id(error), true);    // force
if (ui_sound::played(r)) { /* STARTED or INTERRUPTED */ }
id(ui_sounds).stop();
id(ui_sounds).start();
id(ui_sounds).set_enabled(false);
bool on = id(ui_sounds).is_enabled();
bool busy = id(ui_sounds).is_playing();
```

`PlayResult`: `STARTED`, `INTERRUPTED` (cut a lower-priority cue), `BUSY`, `COOLDOWN`, `MIN_GAP`, `MUTED`,
`DISABLED`, `NOT_STARTED`, `NOT_READY`, `INVALID` (null cue). Only the first two mean the cue reached the player.

## Policy

Checked in this order; the first row that applies decides. A dropped cue is never queued.

| # | Condition | Result | `force` |
| --- | --- | --- | --- |
| 1 | `set_enabled(false)` | `DISABLED` | still dropped |
| 2 | `skip_when_muted` and the player is muted or at volume 0 | `MUTED` | still dropped |
| 3 | the media player is not set up yet | `NOT_READY` | still dropped |
| 4 | `require_start` and `start` not called | `NOT_STARTED` | bypassed |
| 5 | the cue's `cooldown` since its last start is not over | `COOLDOWN` | bypassed |
| 6 | `min_gap` since the last started cue is not over | `MIN_GAP` | bypassed |
| 7 | a cue is playing with priority >= this cue's | `BUSY` | bypassed (it interrupts) |
| 8 | a cue is playing with a lower priority | `INTERRUPTED`: played with `enqueue=false` | same |
| 9 | otherwise | `STARTED` | same |

A cue counts as *playing* when it was issued by this component and one of these holds: it was issued less than
`start_grace` ago, or less than the cue's `duration` ago, or the media player state is `PLAYING` / `ANNOUNCING`; and
never longer than `max_busy` after it was issued.

## Caveats

- The state check uses the whole player. With `announcement: false`, music playing on the same player is replaced by a
  cue (`enqueue=false`) and counts as busy; keep the default for a shared player.
- `set_enabled(false)` does not cut a cue that is already playing; call `ui_sound.stop` for that.
- Priority and the busy window only know about cues played through this component.
- By default there is no mute check: a cue issued while the player is muted is handed to the player, which plays it
  silently, in order, and it takes the busy window like any cue. This keeps "unmute, then play a click" working.
- `skip_when_muted: true` reads the player's mute / volume. The player applies those commands from a queue, so changes
  made in the same loop iteration (for example `unmute` followed by `ui_sound.play`) are not seen yet and the cue is
  dropped with `MUTED`. Use it only if cues are never issued right after a mute / volume change.
- `force` does not override `skip_when_muted` / disabled / not ready.
- Cue files are compiled into flash; keep them short and small (16 kHz mono WAV is plenty).
