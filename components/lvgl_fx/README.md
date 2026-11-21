# lvgl_fx

[Installation](#installation) · [Quick start](#quick-start) · [Configuration](#configuration) ·
[Actions](#actions) · [C++ API](#c-api-for-lambdas) · [Policy](#policy) · [Caveats](#caveats) ·
[Demo](example.yaml)

Motion for ESPHome [LVGL](https://esphome.io/components/lvgl/) dashboards: fades, slides, counting numbers, pulses,
shakes and glows, written the way CSS does it. You say *what* should change (`set` a value, `play` an effect) and the
component decides *how* it moves, whether it may move at all (hidden widgets, popups, boot) and always leaves the screen
in the exact final state.

Tested with ESPHome 2026.11.0 and LVGL 9.6.0 on ESP32 (ESP-IDF).

There are two kinds of motion, like `transition` and `animation` in CSS:

| | **Transition** | **Effect** |
| --- | --- | --- |
| Meaning | a property goes to a new value and stays | a short movement that returns to where it was |
| Examples | show/hide, opacity, gauge value, text, color, x/y, rotation, drawing a line | pulse, shake, nudge, flash, glow, blink |
| Call | `lvgl_fx.set` with the new **value** | `lvgl_fx.play` with the effect **name** |
| Defined in | `transitions:` | `effects:` (a list of `tracks`) |
| Repeats | no | once, N times, or `infinite` (a loop, until `stop`) |

Transitions and effects never fight: a transition only moves the *base* value of a property, an effect only adds an
offset that always ends at zero. Any mix of the two ends on the base value.

## Installation

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ lvgl_fx ]
```

`lvgl_fx` needs the `lvgl:` component. It allocates a fixed pool of `max_targets` x 68 bytes at setup.
Cross-widget completions (`fx::tween`, and the `completion_*` fields of `fx::Opt`) also allocate a temporary record while they run; line draw uses a static pool (see [Line draw](#line-draw)). Nothing is allocated per frame. Until the next release tag contains this component (`@latest` follows the
release tag), use `@master` instead of `@latest`.

## Quick start

No definitions are needed: transitions have defaults and nine effects are built in
(`pulse`, `shake`, `nudge_up`, `nudge_down`, `nudge_left`, `nudge_right`, `flash`, `glow`, `blink`).

```yaml
lvgl_fx:

esphome:
  on_boot:
    - priority: -100
      then:
        - lvgl_fx.start:        # nothing animates before this (boot, first screen build)

lvgl:
  # ... your widgets: popup, bar_fuel, lbl_speed, lbl_prnd ...
  # on_click / on_value / ... of any widget, or any other automation:
  #   - lvgl_fx.set:  { id: popup, visible: true }               # fades in
  #   - lvgl_fx.set:  { id: bar_fuel, value: 72 }                # the bar glides to 72
  #   - lvgl_fx.set:  { id: lbl_speed, text: "88" }              # the new text fades in
  #   - lvgl_fx.play: { id: lbl_prnd, effect: pulse }            # a short pulse
```

A complete device with every feature, using YAML only, is in [`example.yaml`](example.yaml).

## Configuration

Everything below is optional. Times are `200ms`, `1.5s` ... (at most 60 s). `duration` also takes the tokens `fast`,
`normal`, `slow` from `motion`. Percentages are whole numbers (`112%`). `curve` is `linear`, `ease_in`, `ease_out` or
`ease_in_out`.

```yaml
lvgl_fx:
  id: motion_fx                 # optional. Never `fx` (see Caveats)
  motion:
    fast: 120ms                 # the duration tokens (defaults shown)
    normal: 200ms
    slow: 320ms
    intensity: 1.0              # 0 - 4: scales the size of effects (distance, scale, strength), not transitions
  max_targets: 32               # 1 - 255: widgets that can be animated at once (see Caveats)
  slots_in_psram: false         # keep the per-widget state in PSRAM when available
  max_text_runs: 4              # 1 - 32: labels that can type / decode at once (typewriter, decode)
  max_text_length: 128          # 16 - 400: bytes (incl. the NUL) of the longest text they animate
  max_line_runs: 4              # 1 - 32: lines that can be partially drawn at once (draw)
  max_line_points: 16           # 2 - 64: points of the longest line that can be partially drawn
  suppress_when: !lambda return id(popup_open);   # bool; no captures (ids of globals, sensors ... work)

  transitions:
    defaults:                   # per property; every field optional
      visible:  { style: fade, duration: normal, curve: ease_out }
      opacity:  { duration: normal }
      value:    { duration: normal, curve: ease_out }
      text:     { style: fade, duration: fast, from: 85% }
      color:    { duration: normal }
      position: { duration: normal, curve: ease_out }   # x and y
      rotation: { duration: normal, curve: ease_out }
      draw:     { duration: normal, curve: linear }     # an lv_line, drawn along its points
    custom:                     # named transitions: list only what differs from the defaults
      - id: gauge_slow
        value: { duration: slow }
      - id: prnd_move
        position: { duration: 160ms }
        then: prnd_confirm      # an effect played on the same widget when the transition ends
      - id: card
        visible: { show: slide_up, hide: fade, distance: 12 }
      - id: gear_roll
        text: { style: roll, distance: 14, duration: 140ms }
      - id: odometer
        text: { style: count, format: "%.0f km", duration: 800ms }
      - id: terminal_line
        text: { style: typewriter, char_time: 25ms, jitter: true }   # 25 ms per character, whatever the length
      - id: title_decode
        text: { style: decode, char_time: 60ms, noise_period: 50ms }
      - id: trace_in
        draw: { duration: 200ms }
      - id: trace_out
        draw: { duration: 120ms }
      - id: card_logged
        visible: { show: slide_up, hide: fade }
        on_start:               # once per `set` that animates; `x` is the widget (lv_obj_t *)
          - logger.log: { format: "card moving on %p", args: [ x ] }
        on_end:                 # when `then` would play (also when applied at once); `x` is the widget
          - logger.log: { format: "card done on %p", args: [ x ] }

  presets:                      # magnitudes of the built-in effects
    pulse: { scale: 112% }
    nudge_up: { distance: 6 }
    flash: { color: white }

  effect_defaults:              # options for every effect that does not set its own
    retrigger: restart
    when_suppressed: skip
    policy: normal
    defer_timeout: 3s
    cooldown_scope: effect

  effects:
    - id: prnd_confirm
      tracks:
        - flash: { property: bg_color, color: white, duration: 340ms, curve: ease_in }
    - id: heat
      cooldown: 2500ms
      cooldown_scope: widget    # counts from any one-shot effect on the widget, not only `heat`
      on_start:                 # once per play, when the motion starts; `x` is the widget (lv_obj_t *)
        - script.execute: log_heat
      on_end:                   # once, after the last repeat, just before `then`
        - script.execute: heat_done
      tracks:
        - nudge: { y: -2, duration: 220ms }
        - flash: { property: image_recolor, amount: 25%, duration: 220ms }
    - id: warning_blink         # a loop: runs until stopped
      repeat: infinite
      repeat_delay: 400ms
      tracks:
        - blink: { opacity: 35%, duration: slow }
```

### `motion`, `max_targets`, `slots_in_psram`, `max_text_runs`, `max_text_length`, `max_line_runs`, `max_line_points`, `suppress_when`

| Option | Default | |
| --- | --- | --- |
| `motion.fast` / `normal` / `slow` | 120 ms / 200 ms / 320 ms | The duration tokens (they also follow `fx::motion()` at run time). |
| `motion.intensity` | 1.0 | Multiplies effect sizes (nudge/shake distance, pulse growth, flash/glow strength, blink depth). |
| `max_targets` | 32 | Size of the slot pool, 68 bytes per slot (measured, 32-bit target; unchanged by `cooldown_scope`, `on_start` and `on_end`, which live in the const tables). |
| `slots_in_psram` | false | Try PSRAM first and fall back to internal RAM. Accesses per frame are few. |
| `max_text_runs` | 4 | 1 - 32. Labels that can run `typewriter` / `decode` at the same time (a static pool, not per widget). Each run costs `max_text_length` + 28 bytes of RAM (156 bytes by default). |
| `max_text_length` | 128 | 16 - 400. Size of one text run in bytes including the terminating NUL, i.e. the longest text (127 bytes, not characters) that is typed or decoded; longer texts are applied at once. Also 128 bytes of shared scratch. |
| `max_line_runs` | 4 | 1 - 32. Lines that can be *partially drawn* (animating, or resting between 0 and 100 %) at the same time: a static pool, not per widget. A run costs 16 + 8 x `max_line_points` bytes (144 by default). Lines at 0 % or 100 % use no run. |
| `max_line_points` | 16 | 2 - 64. Points of the longest line that can be partially drawn. A longer line, or a line that finds the pool busy, is drawn in whole segments at once (logged once, see [Line draw](#line-draw)). |
| `suppress_when` | none | Lambda returning `bool`. While true, transitions apply instantly and effects follow `when_suppressed`. |

### Transitions

`transitions.defaults` has one entry per property; `transitions.custom` is a list of named transitions (`id:`, any of the
properties, and `then:`). Fields are resolved one by one: the named transition, then `defaults`, then the built-in
value, so a named transition lists only what differs.

| Property | Moves | Fields (built-in default) |
| --- | --- | --- |
| `visible` | show/hide | `style` (`fade`), `show` / `hide`, `from` (90%, 1 - 200%), `distance` (12 px, 0 - 1000), `duration` (`normal`), `curve` (`ease_out`), `delay` (0) |
| `opacity` | opacity | `duration` (`normal`), `curve` (`linear`), `delay` |
| `value` | bar / arc / slider value, image rotation, label number | `duration` (`normal`), `curve` (`ease_out`), `delay` |
| `text` | label text | `style` (`fade`), `from` (85%, 0 - 100%), `distance` (14 px, 0 - 1000), `format` (`%.0f`), `char_time` (none, 1 ms - 5 s), `jitter` (`false`), `noise` (`#%&*+<>/=0123456789`), `noise_period` (50 ms, 10 ms - 1 s), `duration` (`fast`), `curve` (`linear`), `delay` |
| `color` | label text color, image recolor, other widgets' background | `duration` (`normal`), `curve` (`linear`), `delay` |
| `position` | `x` and `y` | `duration` (`normal`), `curve` (`ease_out`), `delay` |
| `rotation` | transform rotation | `duration` (`normal`), `curve` (`ease_out`), `delay` |
| `draw` | how much of an `lv_line` is drawn | `duration` (`normal`), `curve` (`linear`), `delay` |

`duration: 0ms` applies instantly.

**`visible` styles.** The slide and zoom styles add a movement that ends at the normal position, on top of the fade.

| Style | Look |
| --- | --- |
| `fade` | opacity only |
| `slide_up` / `slide_down` / `slide_left` / `slide_right` | opacity + moves `distance` px. The name is the direction of travel when showing: `slide_up` comes in from below. Hiding retraces the path (`slide_up` leaves downwards) |
| `zoom` | opacity + size: grows from `from` to 100% when showing, shrinks to `from` when hiding |
| `none` | instant |

`style` sets both directions; `show` and `hide` set them separately (`show` needs `hide` next to it, `hide` may stand
alone, `style` cannot be combined with either). A hide reversed halfway continues from the current opacity and keeps
the style it started with.

**`text` styles.**

| Style | Look |
| --- | --- |
| `fade` | the new text starts at `from` opacity and rises to 100% |
| `roll` | like `fade`, and the new text moves in `distance` px: from below when the leading number grows, from above when it shrinks. If either text does not start with a number it behaves like `fade` |
| `count` | `set_value` on a label counts from the number on screen to the new one. The label's text must start with the literal text of `format` before its `%`, then a number, otherwise the new text fades in (see Caveats for the limits) |
| `typewriter` | the label is typed out: it shows the first characters of the new text, one more at a time, and ends on the exact text. The label text is the partial text while it types (a cursor is up to the app) |
| `decode` | every character that is not locked yet shows changing noise symbols; the characters lock to the real one from left to right. The label text is the scrambled text while it runs |
| `none` | instant |

**`typewriter` and `decode` in detail.** `duration` is the time of the whole transition, as for every style. For these two
styles the time can instead be given per character with `char_time`, so a longer text takes longer and the pace stays
the same: the duration is `characters x char_time` (at most 60 s). A character is a Unicode code point, never a byte.
`char_time` and `duration` both time the whole transition, so they cannot be given together in one entry (rejected at
configuration time). In a named transition on top of `defaults:` the most specific entry that gives either one wins
(a named `duration` beats the defaults' `char_time` and the other way round); built from C++, `char_time` wins over a
`duration` of the same entry. `delay` and `curve` work as for the other styles: during `delay` the label keeps its old
text, `curve` shapes the speed of the reveal (`linear` types at an even pace).

| Field | Style | Meaning |
| --- | --- | --- |
| `char_time` | both | time per character (1 ms - 5 s) instead of `duration` |
| `jitter` | `typewriter` | `true`: uneven, human-like timing, `false` (default): the same time per character. The total time stays what `duration` or `char_time` says; a space is quicker than other characters and the rhythm is a fixed function of the character and its position, so the same text always types the same way |
| `noise` | `decode` | the symbols that unlocked positions cycle through, default `#%&*+<>/=0123456789`. Non-empty printable ASCII, at most 32 characters |
| `noise_period` | `decode` | time between two noise changes, default 50 ms (10 ms - 1 s). The noise refreshes on its own clock, independent of `duration` and `curve`; the lock-in follows `curve` |

Behavior of both styles:

- The label shows intermediate text while the animation runs, and ends on the exact final text. `stop`, `instant: true`
  and the policy (`suppress_when`, hidden widget, before `start`, reduced motion ...) give the final text at once.
- A new `set` text during the animation **retargets**: the new text starts from scratch (the label restarts from an
  empty text for `typewriter`, from all noise for `decode`). Setting the text the animation is already heading for does
  nothing (it keeps running); setting the text that is on screen right now ends the animation there. An empty text is
  applied at once.
- `decode` only scrambles printable ASCII characters other than space. Spaces, control characters and multi-byte
  characters are shown as they are and are never replaced by noise. The lock-in goes left to right, one code point at a
  time, with equal steps.
- `typewriter` reveals whole code points: it never cuts inside a multi-byte UTF-8 sequence.
- They take no opacity or translate offsets (no layer cost). `on_start` and `then` work as for the other text styles;
  `from`, `distance` are ignored. On other properties `typewriter` and `decode` do not exist (rejected in YAML; from C++
  they behave as `fade` on `visible`).
- `set_value` on a label formats the number and then types it like any text.

`format` is a `printf` format with exactly one floating conversion (`%f`, `%.1f`, `%5.2f`, flags allowed, width at most
40, precision at most 6) and optional `%%` for a literal `%`. Anything else (`%d`, `%s`, `*`, two conversions, none) is
rejected. It formats the number of `lvgl_fx.set ... value:` on a label, with or without `count`.

**`then`** names an effect (built-in or yours) played on the same widget when the transition ends. A transition that
is applied instantly (duration 0, policy, NaN) completes at once, so its `then` plays at once. It does not play when the
call changed nothing (already at that value) or when `stop` ends the transition. The effect goes through the normal
policy; `force` of the `set` call is not passed on.

### Presets

`presets:` changes the magnitudes of a built-in effect, project-wide; effects of your own are not affected. Each preset
needs at least one field and takes `duration` and `curve` (never `delay`) plus the fields of its material:

| Preset | Fields |
| --- | --- |
| `pulse` | `scale` (10 - 400%) |
| `shake` | `distance`, `cycles`, `axis` |
| `nudge_up` / `nudge_down` / `nudge_left` / `nudge_right` | `distance` (px, 0 - 1000; the direction is part of the effect; built-in: 4 px) |
| `flash` | `property`, `color`, `amount` |
| `glow`, `blink` | `opacity` |

### Effects

An effect is a list of `tracks`. **All tracks start together**; give a track a `delay` to put it later. The effect lasts
until its last track ends (`delay + duration`, at most 65535 ms).

| Track | Fields (default) | Moves |
| --- | --- | --- |
| `pulse` | `scale` (110%), `duration` (`normal`) | size: 100% to `scale` and back |
| `nudge` | `x`, `y` (px, at least one), `duration` (`normal`) | position offset: 0 to (x, y) and back |
| `shake` | `distance` (2 px), `cycles` (2), `axis` (`x`), `duration` (`fast`) | position offset: back and forth |
| `flash` | `property`, `color` (`white`), `amount` (100%), `duration` (`slow`) | color: highlight, fading back to the original. `property` is `bg_color`, `text_color` or `image_recolor`; default: text color of a label, recolor of an image, background of anything else. `color` is a name, `0xRRGGBB` or a `color:` id |
| `glow` | `opacity` (50%), `duration` (`slow`) | outline opacity: `opacity` fading to the widget's own |
| `blink` | `opacity` (30%), `duration` (`normal`) | opacity: 100% to `opacity` and back |

Every track also takes `delay` (0) and `curve` (`linear`). Ranges: `scale` 10 - 400%, `amount` and `opacity` 0 - 100%,
`distance`, `x` and `y` up to 1000 px, `cycles` 1 - 100.

Options of an effect (an effect's own value beats `effect_defaults`):

| Option | Values (default) | Meaning |
| --- | --- | --- |
| `repeat` | N, or `infinite` (1) | N plays in a row. `infinite` makes it a **loop**: on until `stop`. In `effect_defaults` only 1 - 65534 (not `infinite`) |
| `repeat_delay` | time (0) | pause between repeats |
| `retrigger` | `restart` / `ignore` | `play` while the same effect runs: start over / do nothing |
| `when_suppressed` | `skip` / `defer` | `play` during `suppress_when`: drop it / keep one (a later one replaces it) and play it when suppression ends |
| `defer_timeout` | time (3s) | how long a deferred effect stays valid |
| `cooldown` | time (0) | minimum time between two plays on a widget (see `cooldown_scope` and Caveats) |
| `cooldown_scope` | `effect` / `widget` (`effect`) | what a `cooldown` counts from: the last start of the *same effect* on the widget / the last start of *any* one-shot effect on the widget |
| `policy` | `normal` / `always` | `always` ignores `suppress_when` |
| `then` | effect | played on the same widget when this effect has ended (all repeats). Not for loops |
| `on_start` | automation | runs when a play of this effect starts moving (see [`on_start`](#on_start)). Effects of your own only; not for loops. Not available in `effect_defaults` |
| `on_end` | automation | runs when a one-shot has ended naturally after all its repeats, at the moment its `then` would play and before it (see [`on_end`](#on_end)). Effects of your own only; not for loops. Not available in `effect_defaults` |

A widget runs at most one one-shot effect and one loop at a time. Playing a second one-shot ends the first (at rest) and
starts the new one; playing a different loop replaces the loop. `cooldown`, `defer_timeout` and `then` do not apply to
loops. Playing a loop that is already on does nothing, so it is safe to repeat the call.

### `on_start`

An `on_start:` automation can be given to an effect (`effects:`) and to a named transition (`transitions: custom:`).
The trigger argument is the target widget, an `lv_obj_t *` named **`x`** in lambdas (like `x` in `on_value`):

```yaml
lvgl_fx:
  effects:
    - id: heat
      tracks:
        - nudge: { y: -2, duration: 220ms }
      on_start:
        - logger.log: { format: "heat started on %p", args: [ x ] }
        - script.execute: count_alert        # or: - lambda: fx::play(x, fx::BLINK);
  transitions:
    custom:
      - id: card
        visible: { show: slide_up, hide: fade }
        on_start:
          - lambda: ESP_LOGD("app", "card %s", lv_obj_is_hidden(x) ? "is hidden" : "is shown");
```

Exactly when it runs:

- **Effect.** Once per play, at the moment the motion starts: after the effect's state is set up, before the first frame.
  Not when the play is dropped (policy), in cooldown, ignored by `retrigger: ignore`, or finds no free slot (see
  `PlayResult` below), and not once per `repeat`. A play kept by `when_suppressed: defer` runs it when it really plays
  later. An effect played through `then` runs its own `on_start` like any other play. `retrigger: restart` (the default)
  starts over, so it runs again.
- **Transition.** Once per `set` call that really animates: not when the change is applied instantly (policy, `duration:
  0`, `style: none`, `instant: true`, the pool is full), nor when the call changes nothing because the property is already
  at (or on its way to) the value. It runs when the animation is started, so a `delay` is still ahead of it. For
  `visible` it runs for showing and for hiding. A `then` effect plays when the transition ends, with its own `on_start`.
- It runs after the runner's bookkeeping is done, so the automation may call `lvgl_fx.play`, `lvgl_fx.stop` or `lvgl_fx.set`
  on the same widget. Keep it short: it runs inside the call that started the motion, on the main loop.
- Do not play the *same* effect from its own `on_start`: with `retrigger: restart` that starts it again, runs the hook
  again, and recurses without bound. The same goes for a transition whose `on_start` sets the same property with that
  same transition.
- `on_start` on a loop (`repeat: infinite`) is rejected at configuration time (hooks on loops are not supported yet).

### `on_end`

`on_end:` takes the same schema as `on_start:` (a list of automations; `x` is the target `lv_obj_t *`) and can be given to
an effect and to a named transition. It runs **at exactly the moments the `then` would be played, and before it**, so
"the thing is over" has one hook that policy cannot drop (a `then` *effect* is judged by effect policy and may be dropped;
`on_end` is not an effect, so it always runs):

```yaml
lvgl_fx:
  transitions:
    custom:
      - id: erase
        draw: { duration: 200ms }
        on_end:
          - lambda: lv_obj_set_hidden(x, true);
```

| | `on_start` | `on_end` | `then` |
| --- | --- | --- | --- |
| The change **animates**, then completes | at the start | at completion | at completion (after `on_end`) |
| Applied **at once by policy** (reduced motion, disabled, before start, hidden ancestor, `suppress_when`), `duration: 0`, `style: none`, a full slot / run pool, or LVGL could not allocate the animation | no | **yes** | yes (its own effect policy applies) |
| `instant: true` | no | no | no |
| **No-op** set (already at, or already moving to, the value) | no | no | no |
| `stop()` | no | no | no |
| **Superseded** by another set on the same property (the old animation is replaced) | no (the new one runs its own) | no (the new set runs its own) | no |
| The app **takes the line over** (`draw`, `lv_line_set_points`) | no | no | no |
| Effect ends after all `repeat`s | at the play's start | **yes** | yes |
| Effect `stop()`, replaced by another one-shot, widget deleted | no | no | no |

- **Transition.** The hook and `then` go through one shared function, so they cannot diverge: whenever a transition
  completes, `on_end` runs first and then `then` is played, and a `completion_effect` of the call (see the C++ API) after
  both. For `visible` it runs after a hide has hidden the widget and after a show. A change that has nothing to
  move (the property already has the value) is a no-op and fires nothing. A `set_draw` that cannot be applied at all (no
  slot, logged) fires nothing either.
- **Effect.** After the last repeat, once the instance has ended (its offsets are gone). A deferred effect fires it when it
  finally plays and ends. A loop never ends, so `on_end` on `repeat: infinite` is rejected at configuration time.
- It runs after the runner's bookkeeping is done, so the automation may call `lvgl_fx.play`, `lvgl_fx.stop` or `lvgl_fx.set`
  on the same widget.
- Recursion caveat (as for `on_start`): a `set` inside `on_end` that ends *instantly* on the *same* transition runs that
  transition's `on_end` again, and recurses without bound; use `instant: true` there (it fires nothing) or another
  transition. Do not play the same effect from its own `on_end` with a `then` back to it either.
- `on_end` alone does not make a transition valid: "a transition that sets nothing" still needs a property or `then`.

### Checked at configuration time

Besides out-of-range values, these are errors:
- `then` on a loop, `then` pointing at a loop (from an effect or a transition), a `then` cycle;
- an unknown effect or transition id; an effect id equal to a built-in effect name; the id `fx` for the component, an
  effect or a transition;
- `on_start` / `on_end` on a loop (`repeat: infinite`); `on_start` / `on_end` in `effect_defaults` or a preset; a `cooldown_scope` other than
  `effect` / `widget`;
- a transition that sets nothing (give it a property or `then`; `on_start` / `on_end` alone do not count); a preset that sets nothing; a `nudge` without `x` and `y`;
  `cycles: 0`;
- `show` without `hide`; `style` together with `show` / `hide`; a `style` that does not fit the property (`slide_up`
  for text, `count` for visible, any `style` on `value`);
- a `format` that is not a single `%f` (with flags, width, precision at most 6, and `%%`);
- `typewriter` / `decode` on a property other than `text`; `char_time` together with `duration` in one entry, or with a
  style other than `typewriter` / `decode`; `jitter` on any style but `typewriter`; `noise` / `noise_period` on any style
  but `decode`; an empty `noise`, one with more than 32 characters or one with anything but printable ASCII;
  `char_time` outside 1 ms - 5 s, `noise_period` outside 10 ms - 1 s;
- `repeat: infinite` in `effect_defaults`; an effect longer than 65535 ms;
- `draw` outside 0% - 100%, written as a bare number (`50`, `0.5`; write `50%`) or on a widget that is not a `line` (also in a
  binding); `max_line_runs` outside 1 - 32, `max_line_points` outside 2 - 64; a `style`, `format` ... on `draw`;
- `lvgl_fx.set` with no or several properties, `lvgl_fx.play` without `effect`.

## Actions

`id` is the id of an LVGL widget. The property values and `force` can be lambdas (trigger arguments such as `x` in
`on_value` are available in them); `effect` and `transition` are ids and fixed.

### `lvgl_fx.set`

Exactly one property per action, plus the optional `transition` (the id of a named transition) and `force`.

| Property | Value | Widgets |
| --- | --- | --- |
| `visible` | bool | any |
| `opacity` | `50%`, `0.5` (a fraction), or a lambda returning 0 - 255 | any |
| `value` | number | bar, arc, slider (clamped to the range); image: rotation in degrees (the short way round); label: the number, through `format` / `count` |
| `text` | string, or `format` + `args` | label |
| `color` | name, `0xRRGGBB`, `color:` id | label: text color; image: recolor; others: background |
| `draw` | percentage `0%` - `100%` (float allowed: `33.3%`), or a lambda returning 0 - 100 | line (see [Line draw](#line-draw)) |
| `x`, `y` | integer pixels | any. If the widget's current `x`/`y` is a percentage (or C++ passes `lv_pct()`), the change is instant: percentages cannot be interpolated |
| `rotation` | degrees | any (transform rotation; set `transform_pivot_x/y` yourself) |

```yaml
- lvgl_fx.set: { id: popup, visible: true, transition: card }          # `card` is a transition id
- lvgl_fx.set: { id: bar_fuel, value: 72, transition: gauge_slow }
- lvgl_fx.set: { id: lbl_gear, text: "4", transition: gear_roll }      # rolls in from below
- lvgl_fx.set: { id: lbl_odo, value: 1250, transition: odometer }      # counts to "1250 km"
- lvgl_fx.set: { id: box, x: 170, transition: prnd_move }              # moves, then plays prnd_confirm
- lvgl_fx.set: { id: bar_fuel, value: 0, instant: true }              # cancel the value glide and apply now
- lvgl_fx.set: { id: splash, visible: false, force: true }             # even before lvgl_fx.start
- lvgl_fx.set: { id: img_needle, value: 90 }                           # an image rotates
- lvgl_fx.set: { id: route, draw: 100%, transition: trace_in }          # a line is drawn along its points
- lvgl_fx.set: { id: route, draw: 0%, transition: trace_out }           # ... and erased again
- lvgl_fx.set: { id: box, color: 0xC62828 }
```

With trigger arguments and lambdas:

```yaml
sensor:
  - platform: template
    id: speed
    update_interval: never
    on_value:
      then:
        - lvgl_fx.set:
            id: bar_speed
            value: !lambda return x;
        - lvgl_fx.set:
            id: lbl_speed
            text:
              format: "%.0f km/h"
              args: [ x ]
            force: !lambda return x > 200;
        - lvgl_fx.set: { id: lbl_speed, opacity: !lambda "return x > 50 ? 255 : 100;" }
```

Setting a property to the value it already has (or is already moving to) does nothing, and costs next to nothing, so a
sensor can call it every second.

### `lvgl_fx.play` and `lvgl_fx.stop`

```yaml
- lvgl_fx.play: { id: lbl_prnd, effect: pulse }               # built-in
- lvgl_fx.play: { id: icon, effect: heat }                    # yours
- lvgl_fx.play: { id: icon, effect: warning_blink }           # a loop: on
- lvgl_fx.play: { id: splash_logo, effect: glow, force: true }
- lvgl_fx.stop: { id: icon, effect: warning_blink }           # just this effect (running, loop or deferred)
- lvgl_fx.stop: icon                                          # everything on the widget
```

`play` takes `id`, `effect` and `force`; there is no per-call transition or duration. `stop` ends the motion and sets the
**final state at once**: transitions jump to their target (a running hide completes), effects return to rest, a count
shows the final number, deferred effects are dropped. `then` is not played by `stop`.

### `lvgl_fx.set_enabled` and `lvgl_fx.set_reduced_motion`

Policy switches (row 1 of the policy table), for example from a settings screen. A bool, or a lambda returning one.
Disabled or reduced motion applies every transition instantly and drops effects from the next call on. Motion
already running finishes; loops go to rest and restart from the beginning when motion is allowed again.

```yaml
- lvgl_fx.set_reduced_motion: true
- lvgl_fx.set_enabled: !lambda return id(animations_on);
```

### `lvgl_fx.start`

Declares the UI ready. Until then transitions apply instantly and effects are dropped. Call it once the screens are built:

```yaml
- lvgl_fx.start:                 # or `- lvgl_fx.start: motion_fx` with the component id
```

## C++ API for lambdas

```cpp
// namespace fx = esphome::lvgl_fx; is declared for you
fx::set_visible(id(popup), true);
fx::set_opacity(id(popup), 128);
fx::set_value(id(bar_fuel), x);                        // float
fx::set_text(id(lbl_speed), "42");
fx::set_color(id(lbl_speed), lv_color_hex(0xFF0000));
fx::set_x(id(box), 170);
fx::set_y(id(box), 20);
fx::set_rotation(id(box), 90.0f);
fx::set_draw(id(route), 100.0f);                       // percent of the line's path length (an lv_line)

fx::play(id(lbl_prnd), fx::PULSE);                     // fx::SHAKE, NUDGE_UP/DOWN/LEFT/RIGHT, FLASH, GLOW, BLINK
fx::play(id(icon), id(heat));                          // an effect of your configuration
fx::stop(id(icon));                                    // everything
fx::stop(id(icon), id(warning_blink));                 // one effect
fx::start();
bool ready = fx::is_ready();                           // true once start() (or `lvgl_fx.start`) has run
```

The first argument is an `lv_obj_t *` or an ESPHome LVGL widget. Every `set_*` and `play` takes a last `fx::Opt` (including `instant` for setters):

For a `set` action or C++ setter, `instant: true` / `{.instant = true}` cancels the
current animation of that property (including a same-target animation), applies
the requested value immediately, and does not play the transition’s `then`.
Other properties and effects keep running. Visibility and opacity share an
animation channel: an instant opacity update cancels a pending show/hide animation
and completes a pending hide immediately. `instant` can be templated on YAML
`lvgl_fx.set`; it is not an option for `lvgl_fx.play`. NaN handling is unchanged.

```cpp
fx::set_value(id(bar_fuel), x, fx::Opt{id(gauge_slow)});                 // named transition
fx::set_x(id(box), 170, {.transition = id(prnd_move), .force = true});
fx::play(id(icon), fx::PULSE, {.force = true});
```

**Completion on another widget.** `fx::Opt` has the two fields of `fx::TweenOpt`, `completion_effect` and `completion_target`,
for **every** `set_*` (not for `play`, which has no transition). When the transition really completes, the effect
plays on the target with the default policy, after the transition's own `then` (which plays on the widget itself):

```cpp
// the route is drawn; when the last point is reached the end marker pulses
fx::set_draw(id(route), 100.0f, {.transition = id(trace_in), .completion_effect = fx::PULSE,
                                 .completion_target = id(end_marker)});
fx::set_value(id(bar_fuel), 20.0f, {.completion_effect = id(heat), .completion_target = id(icon)});
```

| What happens to the transition | Completion effect |
| --- | --- |
| it ends by itself (the animation reaches its end) | **plays** |
| applied instantly: policy (disabled, hidden, before `start`, `suppress_when`), `duration: 0`, `instant: true`, no free slot or run | does not play (unlike `then`, which plays after an instant application: nothing *moved* here, so nothing completed) |
| `stop()`, replaced by another `set` of the same property, cancelled | does not play |
| the call changed nothing (already at that value) | nothing is armed; a running transition keeps the completion it started with |
| the target widget is deleted | disarmed (no crash, no effect); the transition itself goes on |
| the source widget is deleted | gone with it |

Both fields must be set; one alone is ignored. The effect is played with the default policy (not `force`, not the
call's transition), so it can be dropped like any `play`. Each call that sets both allocates one small record (a few
words) for as long as the transition runs and registers a delete hook on the target; if that fails the transition
runs without the follow-up effect. `fx::cancel_tween_effect(target)` also disarms the pending `Opt` completions that
target a widget. C++ only: in YAML use `then:` (same widget) or `on_start:`.

**`play` tells what happened.** It returns an `fx::PlayResult`; ignoring it is fine (the YAML action does):

```cpp
fx::PlayResult r = fx::play(id(icon), id(heat));
if (r == fx::PlayResult::DROPPED) ESP_LOGD("app", "no motion: the screen is not ready or is hidden");
```

| Value | Meaning |
| --- | --- |
| `STARTED` | the effect is moving now (one-shot started, or loop switched on and moving) |
| `DEFERRED` | `suppress_when` is true and the effect has `when_suppressed: defer`: it is kept and plays later |
| `REMEMBERED` | a loop is now on but at rest because policy forbids motion for the moment (disabled, reduced motion, before `start`, hidden, suppressed); it starts by itself |
| `IGNORED` | already running: `retrigger: ignore` on a one-shot that is running, or the same loop is already on |
| `DROPPED` | policy dropped a one-shot: disabled, reduced motion, before `start`, hidden widget or ancestor, `suppress_when` with `skip` |
| `COOLDOWN` | the `cooldown` is not over |
| `NO_SLOT` | the slot pool is full (`max_targets`), or LVGL could not allocate the animation |
| `INVALID` | no widget or effect given, or an effect without any motion (no track has a duration) |

`force` skips `DROPPED`, `DEFERRED` and `COOLDOWN`. `fx::is_ready()` tells whether `start()` has been called.

`id(heat)` / `id(gauge_slow)` are the ids of your `effects:` / `transitions: custom:` entries (a mistyped id is a compile
error).

**Runtime switches.** `set_enabled` and `set_reduced_motion` also exist as YAML actions; the rest is C++ only:

```cpp
fx::set_enabled(false);                  // from the next call: applied instantly / dropped; running motion finishes
                                         // (YAML: lvgl_fx.set_enabled / lvgl_fx.set_reduced_motion)
fx::set_reduced_motion(true);            // same, for an accessibility setting (loops are remembered)
fx::motion().intensity = 0.6f;           // effect sizes; applies at the next play(), to all running effects
fx::effect_defaults().cooldown = 500;    // the same options as `effect_defaults:`
static const fx::Track bigger = {.kind = fx::TrackKind::PULSE, .scale = 125};
fx::set_preset(fx::PULSE, &bigger);      // like `presets:`; nullptr restores the built-in values
```

**`fx::tween`** animates anything with a setter, under the same policy (instant when not ready, disabled, suppressed or
when `target` is hidden):

```cpp
fx::TweenOpt opt;
opt.duration = 300;                      // 0 = motion().normal
opt.curve = fx::Curve::EASE_OUT;
opt.target = id(my_arc);                 // optional: policy "hidden" applies to this widget
fx::tween(id(my_arc), [](void *var, int32_t v) { lv_arc_set_value(static_cast<lv_obj_t *>(var), v); }, 0, 100, opt);
```

Starting a tween on the same `(var, setter)` replaces the running one; it starts from the `from` you pass. `var` must
outlive the tween. `fx::cancel_tween(var, setter)` stops it without applying the end value.

Effects and transitions can also be written as `static const` tables (`fx::EffectDef`, `fx::Track`,
`fx::TransitionDef`, `fx::PropTransition`) in a lambda; fields you leave out take the defaults. `cooldown_scope` and
`on_start` / `on_end` are fields of `fx::EffectDef` and of `fx::TransitionDef` (`on_end` is the last field of both,
after `on_start`): a plain function pointer `void (*)(lv_obj_t *)`, so a captureless lambda fits. See
`tests/components/lvgl_fx/test.esp32-idf.yaml` for the full form.

## Line draw

`draw` progressively draws or erases an `lv_line` along its points, by **path length**: `0%` draws nothing, `100%` is the
whole polyline, `50%` is half of the total length (the last visible segment is cut part-way). It is a transition like any
other, with its own entry in `transitions:` (`duration`, `delay`, `curve`; `linear` by default so the tip moves at an even
speed), named transitions, bindings, `instant`, `force`, `on_start`, `on_end`, `then` and the policy table.

```yaml
lvgl_fx:
  transitions:
    defaults:
      draw: { duration: 200ms }
    custom:
      - id: trace_in
        draw: { duration: 200ms, curve: linear }
      - id: trace_out
        draw: { duration: 120ms }
    bindings:
      - { id: route, draw: trace_in }          # `lvgl_fx.set draw` and fx::set_draw on `route` use trace_in by default

lvgl:
  pages:
    - widgets:
        - line:
            id: route
            points: [ { x: 0, y: 0 }, { x: 40, y: 0 }, { x: 40, y: 30 }, { x: 80, y: 30 } ]
# ...
- lvgl_fx.set: { id: route, draw: 100% }                                   # drawn in
- lvgl_fx.set: { id: route, draw: 0%, transition: trace_out }              # erased
- lvgl_fx.set: { id: route, draw: !lambda "return x;" }                    # any 0 - 100 number
```

```cpp
fx::set_draw(id(route), 62.5f);                                            // lv_obj_t * or an ESPHome line widget
fx::set_draw(id(route), 100.0f, {.transition = id(trace_in), .completion_effect = fx::PULSE,
                                 .completion_target = id(end_marker)});    // pulse another widget when it is drawn
```

**What it does, exactly.**

- **Final state.** The state after a `set draw` is exact and independent of the motion: at `100%` the line shows its own
  full points array again (the array the app gave LVGL, untouched), at `0%` it shows nothing, in between it shows the polyline up to that
  length. `stop()` jumps to the target percent. A tiny percentage shows a tiny segment (with `line_rounded` a dot).
- **Independent of `visible`.** Erasing to `0%` does not hide the line, and showing a hidden line does not draw it. The
  line is merely invisible at 0 %; the app decides about `hidden`, see the recipe below. `draw` on a hidden line is
  applied instantly (policy row 3), so set `visible` first.
- **Not a line.** `fx::set_draw` on any other widget logs once and does nothing (YAML rejects it at configuration time).
- **NaN / non-finite.** Instantly 0 (erased), like `value` goes to its minimum. Values outside 0 - 100 are clamped.
- **Same value.** Setting the percentage the line has (or is heading for) does nothing; a running draw keeps going.
  A new target while drawing continues from the percentage on screen. `instant` cancels the motion and applies the value.
- **Fewer than 2 points** (or more than 255): logged once, nothing to draw, nothing changes.

**The full points.** fx never owns the line's points. The first time a line is drawn it reads them back with
`lv_line_get_points` / `lv_line_get_point_count` and keeps only the pointer. LVGL's own rule applies: **the array must
outlive the line** (a `static` array, or the widget's own storage). If the app later calls `lv_line_set_points` with a
different array - also while the line is erased, partially drawn or animating - fx notices (the line's points pointer is neither fx's
partial buffer nor the array it captured), drops its state, ends a running draw without a completion, and
captures the new array as the full line (100%). A new array at the same address *and* with the same point count as
the old one cannot be told apart; the percentage fx remembers then still applies.

**YAML `line` widgets.** ESPHome's `line` keeps its points in a `FixedVector` inside the widget (`LvLineType::points_`) and
passes its storage to `lv_line_set_points`, so `lv_line_get_points` returns that storage and it lives as long as the
widget: nothing needs to be done. `lvgl.line.update: { id: route, points: [...] }` replaces the vector and calls
`lv_line_set_points` with the new storage, which is the re-capture case above. Point coordinates written as percentages
(`x: 50%`) are measured against the line's size when it is drawn.

**The partial array.** LVGL draws exactly the array it is given, so a partly drawn line is a partial array, in a static
pool like the text runs of `typewriter`/`decode`: `max_line_runs` records of `max_line_points` points (BSS, no heap).

| State of the line | Points LVGL has | Costs |
| --- | --- | --- |
| `100%` (at rest) | the app's full array | nothing: not tracked |
| animating, or resting at `0% < p < 100%` | fx's buffer in a run (the run stays bound to the line) | one run |
| `0%` (at rest) | the full array with **one** point (LVGL draws nothing for fewer than 2) | no run; the full pointer and point count are kept in the slot |

A run is free as soon as the line is no longer partially drawn: at exactly 100 % the line gets its full points back, at 0 %
the run is released too, and deleting the line frees it, so erased and complete lines use no run. A line that is **erased
or partly drawn needs a target slot** (`max_targets`, 68 bytes like every animated widget): a pool of slots in use makes the
call do nothing, logged once.

**When the pool cannot serve a line** (every run is busy, or the line has more than `max_line_points` points) the call
still applies its final state at once, without motion, and logs once: `0%` and `100%` stay exact, anything in between shows
the **whole segments** that fit into that length (no interpolated tip). Its `then` plays, its completion effect does not
(nothing moved). Raise `max_line_runs` / `max_line_points` if this log appears.

**Layout.** LVGL sizes a line with the default `width`/`height: size_content` from the points it is given, so a partly
drawn line is smaller than the full one. Drawing itself is unaffected (it uses absolute coordinates), but if the line is
aligned, centered or laid out in a container, give it a fixed `width` and `height` so that its place does not change
while it is drawn.

**Recipe: draw in on show, erase then hide.** `draw` and `visible` are separate, so chain them yourself. Drawing needs
the line to be shown (a hidden widget applies instantly), and the line must start at 0 % to draw in:

```yaml
lvgl_fx:
  transitions:
    custom:
      - id: trace_in
        draw: { duration: 250ms }
      - id: trace_out
        draw: { duration: 120ms }
        on_end:                      # runs when the erase is over
          - lambda: lv_obj_set_hidden(x, true);

script:
  - id: show_route
    then:
      - lvgl_fx.set: { id: route, draw: 0%, instant: true }               # start empty (a hidden line: instantly)
      - lvgl_fx.set: { id: route, visible: true, instant: true }
      - lvgl_fx.set: { id: route, draw: 100%, transition: trace_in }      # the tip runs along the route
  - id: hide_route
    then:
      - lvgl_fx.set: { id: route, draw: 0%, transition: trace_out }       # erased, then `on_end` hides it
```

The same from C++: `fx::set_draw(line, 0, {.instant = true}); fx::set_visible(line, true, {.instant = true});
fx::set_draw(line, 100, {.transition = id(trace_in)});`.

**Why this is exact.** `on_end` fires when the erase animation completes, and also when the erase is applied at once
(motion disabled, before `lvgl_fx.start`, a hidden ancestor, `suppress_when`, `duration: 0`, a full run pool): the line is
hidden exactly when the erase is over, in every case. It does not fire when the call changes nothing (the line is already
empty), on `stop()`, with `instant: true`, when another `draw` call replaces the erase, or when the app takes the line
over, so none of those hides a line that was not erased. A `then` effect cannot do this job: it is played with the normal
effect policy and is dropped while motion is not allowed, so the line would stay visible. To pulse *another* widget when the
route is complete, use `completion_effect` / `completion_target` (see the C++ API).

## Policy

Every transition and one-shot effect is judged once, when it starts. The first row that applies wins:

| # | Condition | Transition | One-shot effect | Loop |
| --- | --- | --- | --- | --- |
| 1 | disabled (`lvgl_fx.set_enabled`) or reduced motion (`lvgl_fx.set_reduced_motion`) | applied instantly | dropped | remembered as on |
| 2 | before `lvgl_fx.start` | applied instantly | dropped | remembered, starts at `start` |
| 3 | the widget or an ancestor is hidden | applied instantly | dropped | rests; resumes when shown |
| 4 | `suppress_when` is true (not for `policy: always`) | applied instantly | `when_suppressed`: dropped or deferred | rests; resumes when released |
| 5 | `cooldown` not over (see `cooldown_scope`) | n/a | dropped | n/a |

- `force: true` skips rows 1 - 5. A loop started with `force` is never paused by policy.
- "Applied instantly" always lands on the exact final value, so nothing is lost; `set_visible` only looks at the
  *ancestors* for row 3 (the widget's own hidden flag is what it changes).
- A loop that rests starts again from its first phase, not where it stopped. A deferred effect plays when suppression
  ends if still within `defer_timeout`; the other rows are then judged again.
- `suppress_when` is evaluated at most once per main-loop iteration and cached. A flag changed in the same iteration it is
  read may still be seen with its old value until the next one.
- A transition that is replaced by another on the same property continues from where it is.
- Once fx has taken over opacity, scale, rotation, color or outline of a widget, change them through fx only. Value,
  text and x/y are read again on every call, so changing them elsewhere is fine.
  A line's points are the exception to "read again": fx notices a new array given with `lv_line_set_points` and
  captures it (see [Line draw](#line-draw)); leave a partly drawn line's array alone otherwise.

## Caveats

- **`max_targets` is a capacity.** A widget takes a slot the first time something needs one (visible, opacity, color or
  rotation transitions, animated text, any effect or loop) and **keeps it until the widget is deleted**; finished motion
  does not free it. `value`, `x` and `y` transitions need none. When all slots are taken a log line is printed once,
  transitions on further widgets apply instantly and effects on them are skipped. Count your animated widgets (pages
  that stay alive count too) and set `max_targets` at least that high. A slot is 68 bytes (measured).
- **Effects own `translate`.** `nudge`, `shake`, the slide styles and `roll` write `translate_x/y`. Do not set
  `translate_*` yourself on a widget that uses them; use `x`/`y` (and `lvgl_fx.set x/y`) for positions.
- **Glow needs an outline.** Give the widget `outline_width` and `outline_color`, and **`outline_opa: 0` set on the widget
  itself**. The LVGL default for `outline_opa` is fully opaque, which leaves nothing to glow; fx warns once in the log.
  For a focused or pressed widget only the widget's own default-state style is used as the resting opacity, so a theme or
  shared style is not enough.
- **Scale and rotation go through a layer.** `pulse`, the `zoom` visible style, `rotation:` and image rotation make LVGL
  render the widget into an intermediate layer, which is expensive on large areas. A warning is logged once when fx
  takes over scale or rotation of a target covering a quarter of the screen or more. Plain opacity (`opacity`, fades,
  `blink`) and `translate`/color/outline motion (`nudge`, `shake`, `slide`, `flash`, `glow`) do not need a layer.
- **Pivot.** When fx first takes over the scale of a widget (pulse, zoom) and the widget is not scaled, rotated or given a
  pivot already, it sets `transform_pivot` to 50% / 50% so it grows from the middle. Anything else is left alone.
  `set_rotation` / `rotation:` never set a pivot: add `transform_pivot_x` / `transform_pivot_y` to the widget. (An
  image's `value` rotation uses the image's own `pivot_x` / `pivot_y`.)
- **Untinted images.** An image with recolor opacity 0 has no tint; the first `set_color` turns the tint on at the new color
  **instantly** (it cannot fade from the black default). Later changes fade. A `flash` on such an image fades the tint
  in and out.
- **One color channel per widget.** `set_color` and `flash` share it (label: text color, image: recolor, other: background).
  A `flash` with another `property` takes the channel over when nothing else uses it; while a color transition runs it
  uses the busy channel and a warning is logged.
- **`count` limits.** Only `%f` formats with a precision of at most 6 can count, the numbers must be below 2·10⁹ in fixed
  point, and the difference between start and end must be at most 2,097,150 fixed-point units (`%.0f`: 2,097,150;
  `%.2f`: 20,971.50). Otherwise the new text fades in (the final text is the same). A label with no readable number
  (empty, `--`, text) also fades instead of counting. Formatted label numbers are cut at 47 characters.
- **`cooldown`** counts from the start of the last one-shot effect on the same widget; `force` ignores it, loops are not
  affected. With `cooldown_scope: effect` (the default) it only counts when that was the same effect: `heat`, `pulse`,
  `heat` plays `heat` again. With `cooldown_scope: widget` any effect counts: `heat`, `pulse` (cooldown 2500 ms) is
  dropped. The cooldown that applies is the one of the effect being played. A `then` follow-up is not subject to cooldown (it is part of the same play); the other policy rows still apply to it.
- **`intensity`** is global and sampled when an effect starts: changing it applies at the next `play()` to all effects
  that are running as well.
- **`then` is played with the default policy**, not with the `force` or transition of the call that triggered it.
- **Ids named `fx`.** `fx` is the C++ alias of the namespace, so no id in the whole configuration may be `fx` (the
  component, effect and transition ids are rejected at validation; other platforms' ids would fail to compile).
- **`typewriter` / `decode` limits.** The final text lives in a fixed record while the label shows something else, so
  there is a small static pool: `max_text_runs` records (4) of `max_text_length` bytes (128: texts up to
  127 bytes, not characters). At most that many labels can type or decode at once; a longer text, or a label that
  finds the pool busy, gets the final text at once (logged once) and nothing else changes. Both are
  YAML options (C++-only users can define `LVGL_FX_TEXT_RUNS` / `LVGL_FX_TEXT_RUN_BYTES`). Each visible change is one `lv_label_set_text`, so LVGL reallocates the label's own buffer
  (as `count` does); the typewriter changes the text at most once per character and `decode` at most once per
  `noise_period`. Anything that changes the label with `lv_label_set_text` while one of these runs is overwritten by the
  next frame; use `lvgl_fx.set` (or `stop` first).
- **`draw` limits.** One draw per line; the state lives in `Slot::style`, so `draw` on a line and the text styles never
  meet (a line is not a label). Lines of 2 - 255 points; partial drawing needs a run (`max_line_runs` /
  `max_line_points`), otherwise whole segments are shown. Points are read when the line is first drawn: change a
  line's points through `lv_line_set_points` (fx re-captures) and never edit a captured array in place while the line is
  partly drawn. `lv_line_set_points_mutable` arrays are restored as mutable at 100 %.
- **Not yet supported:** user-defined tracks, stagger over several widgets, binding a
  transition to a widget instead of naming it per call.

## Footprint

Definitions become `static const` tables in flash. RAM is the slot pool (`max_targets` x 68 bytes, 2.2 KB for the
default 32) plus one LVGL animation per running transition or effect, regardless of the number of tracks. The
`typewriter` / `decode` styles add a static pool of `max_text_runs` x (`max_text_length` + 28) bytes of text runs, 4 x 156 bytes by default, plus `max_text_length` bytes of scratch, which is not
part of the slot (still 68 bytes). Line draw adds a static pool of `max_line_runs` x (16 + 8 x `max_line_points`) bytes (4 x 144
by default, 576 bytes) and keeps its per-line state in the existing slot (`Slot::style`, which a line does not
otherwise use): the slot stays 68 bytes. Nothing is allocated per frame. The compile test and the demo build on ESP32 with ESP-IDF.


### Additional tracks and completion hooks

A flash accepts `profile: triangle` for a base → highlight → base ramp. The default
`fade_out` keeps the original immediate highlight. An `image_pulse` track adds its
`amount` (percentage points, default 30%) to the image opacity base and eases back
to that base over its duration. Use `fx::set_image_opa(widget, opacity)` to update
that base while effects are active. `fx::stop` restores it. Image pulse amount is
absolute and is not affected by motion intensity.

Named transitions accept `policy: always`; this bypasses only `suppress_when`,
while readiness, hidden ancestors, enabled and reduced-motion policy still apply.
`fx::TweenOpt::always` does the same for user tweens. To follow an actual tween
completion on another widget, set `completion_effect` and `completion_target`.
The existing `on_done` runs first; cancellation or an instant application does not
play the completion effect. `fx::cancel_tween_effect(target)` disarms pending
completions without stopping the source tween. The target is disarmed on deletion.
Cross-widget completions allocate a small temporary record and register a deletion
callback; if allocation fails the source tween and its callback still run, without
the follow-up effect. The same two fields exist on `fx::Opt` for every `set_*`
(see the C++ API).


### Widget transition bindings

A binding assigns a named transition to a widget and a property. Explicit
`transition` options still win as a whole; otherwise the binding supplies that
property's transition, policy and `then`, followed by the normal global and
built-in field defaults. Other properties on the same widget are unaffected.

```yaml
lvgl_fx:
  transitions:
    custom:
      - id: needle_motion
        value: { duration: 220ms }
    bindings:
      - { id: needle, value: needle_motion }
```

Bindings work for YAML actions and C++ setters. `position` applies to both x and
y; a label value uses `text`, while an image value uses `value`. The widget and
transition IDs must exist, and a widget/property pair may be bound only once.
Static records do not consume motion slots; deleting a widget disarms its binding.
