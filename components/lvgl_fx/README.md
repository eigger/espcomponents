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
| Examples | show/hide, opacity, gauge value, text, color, x/y, rotation | pulse, shake, nudge, flash, glow, blink |
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
Cross-widget tween completions also allocate a temporary record; nothing is allocated per frame. Until the next release tag contains this component (`@latest` follows the
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

  presets:                      # magnitudes of the built-in effects
    pulse: { scale: 112% }
    nudge_up: { distance: 6 }
    flash: { color: white }

  effect_defaults:              # options for every effect that does not set its own
    retrigger: restart
    when_suppressed: skip
    policy: normal
    defer_timeout: 3s

  effects:
    - id: prnd_confirm
      tracks:
        - flash: { property: bg_color, color: white, duration: 340ms, curve: ease_in }
    - id: heat
      cooldown: 2500ms
      tracks:
        - nudge: { y: -2, duration: 220ms }
        - flash: { property: image_recolor, amount: 25%, duration: 220ms }
    - id: warning_blink         # a loop: runs until stopped
      repeat: infinite
      repeat_delay: 400ms
      tracks:
        - blink: { opacity: 35%, duration: slow }
```

### `motion`, `max_targets`, `slots_in_psram`, `suppress_when`

| Option | Default | |
| --- | --- | --- |
| `motion.fast` / `normal` / `slow` | 120 ms / 200 ms / 320 ms | The duration tokens (they also follow `fx::motion()` at run time). |
| `motion.intensity` | 1.0 | Multiplies effect sizes (nudge/shake distance, pulse growth, flash/glow strength, blink depth). |
| `max_targets` | 32 | Size of the slot pool, 68 bytes per slot (measured, 32-bit target). |
| `slots_in_psram` | false | Try PSRAM first and fall back to internal RAM. Accesses per frame are few. |
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
| `text` | label text | `style` (`fade`), `from` (85%, 0 - 100%), `distance` (14 px, 0 - 1000), `format` (`%.0f`), `duration` (`fast`), `curve` (`linear`), `delay` |
| `color` | label text color, image recolor, other widgets' background | `duration` (`normal`), `curve` (`linear`), `delay` |
| `position` | `x` and `y` | `duration` (`normal`), `curve` (`ease_out`), `delay` |
| `rotation` | transform rotation | `duration` (`normal`), `curve` (`ease_out`), `delay` |

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
| `none` | instant |

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
| `cooldown` | time (0) | minimum time between two plays of this effect on a widget (see Caveats) |
| `policy` | `normal` / `always` | `always` ignores `suppress_when` |
| `then` | effect | played on the same widget when this effect has ended (all repeats). Not for loops |

A widget runs at most one one-shot effect and one loop at a time. Playing a second one-shot ends the first (at rest) and
starts the new one; playing a different loop replaces the loop. `cooldown`, `defer_timeout` and `then` do not apply to
loops. Playing a loop that is already on does nothing, so it is safe to repeat the call.

### Checked at configuration time

Besides out-of-range values, these are errors:
- `then` on a loop, `then` pointing at a loop (from an effect or a transition), a `then` cycle;
- an unknown effect or transition id; an effect id equal to a built-in effect name; the id `fx` for the component, an
  effect or a transition;
- a transition that sets nothing (give it a property or `then`); a preset that sets nothing; a `nudge` without `x` and `y`;
  `cycles: 0`;
- `show` without `hide`; `style` together with `show` / `hide`; a `style` that does not fit the property (`slide_up`
  for text, `count` for visible, any `style` on `value`);
- a `format` that is not a single `%f` (with flags, width, precision at most 6, and `%%`);
- `repeat: infinite` in `effect_defaults`; an effect longer than 65535 ms;
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

fx::play(id(lbl_prnd), fx::PULSE);                     // fx::SHAKE, NUDGE_UP/DOWN/LEFT/RIGHT, FLASH, GLOW, BLINK
fx::play(id(icon), id(heat));                          // an effect of your configuration
fx::stop(id(icon));                                    // everything
fx::stop(id(icon), id(warning_blink));                 // one effect
fx::start();
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
`fx::TransitionDef`, `fx::PropTransition`) in a lambda; fields you leave out take the defaults. See
`tests/components/lvgl_fx/test.esp32-idf.yaml` for the full form.

## Policy

Every transition and one-shot effect is judged once, when it starts. The first row that applies wins:

| # | Condition | Transition | One-shot effect | Loop |
| --- | --- | --- | --- | --- |
| 1 | disabled (`lvgl_fx.set_enabled`) or reduced motion (`lvgl_fx.set_reduced_motion`) | applied instantly | dropped | remembered as on |
| 2 | before `lvgl_fx.start` | applied instantly | dropped | remembered, starts at `start` |
| 3 | the widget or an ancestor is hidden | applied instantly | dropped | rests; resumes when shown |
| 4 | `suppress_when` is true (not for `policy: always`) | applied instantly | `when_suppressed`: dropped or deferred | rests; resumes when released |
| 5 | `cooldown` not over | n/a | dropped | n/a |

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
- **`cooldown`** compares with the last one-shot effect on the same widget, and only when that was the same effect:
  `heat`, `pulse`, `heat` plays `heat` again. It counts from the start of that play, and `force` ignores it.
- **`intensity`** is global and sampled when an effect starts: changing it applies at the next `play()` to all effects
  that are running as well.
- **`then` is played with the default policy**, not with the `force` or transition of the call that triggered it.
- **Ids named `fx`.** `fx` is the C++ alias of the namespace, so no id in the whole configuration may be `fx` (the
  component, effect and transition ids are rejected at validation; other platforms' ids would fail to compile).
- **Not yet supported:** a `typewriter` text style, user-defined tracks, stagger over several widgets, binding a
  transition to a widget instead of naming it per call.

## Footprint

Definitions become `static const` tables in flash. RAM is the slot pool (`max_targets` x 68 bytes, 2.2 KB for the
default 32) plus one LVGL animation per running transition or effect, regardless of the number of tracks. Nothing is
allocated per frame. The compile test and the demo build on ESP32 with ESP-IDF.


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
the follow-up effect.


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
