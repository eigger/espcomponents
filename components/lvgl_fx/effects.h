#pragma once

// Effect definitions and the per-track frame math.
//
// An effect is a const table: an array of tracks plus options.  P4 codegen emits these tables into flash; every field
// has an "unset" default, so a generated definition only lists what the YAML says (designated initializers, in
// declaration order):
//
//   static const fx::Track heat_tracks[] = {{.kind = fx::TrackKind::NUDGE, .duration = 220, .y = -2}};
//   static const fx::EffectDef heat = {.tracks = heat_tracks, .track_count = 1, .cooldown = 2500};
//
// Unset track fields resolve at run time against the built-in values (the 4.3 table); built-in effects additionally
// resolve against their preset (4.4 `presets`, see set_preset()).  Unset effect options resolve against the global
// effect defaults (`effect_defaults()`).  Nothing is copied into RAM.

#include <cstddef>
#include <cstdint>

#include "props.h"

namespace esphome {
namespace lvgl_fx {

// ---- curves and durations ------------------------------------------------------------------------------------------

// DEFAULT only means "not given" in an effect track (it behaves as LINEAR everywhere).
enum class Curve : uint8_t { LINEAR, EASE_IN, EASE_OUT, EASE_IN_OUT, DEFAULT };

// LVGL path callback for a curve (used for lv_anim based motion).
lv_anim_path_cb_t curve_path(Curve curve);
// Integer curve evaluation for runners that cut sub-ranges out of one lv_anim.  t and the result are in
// [0, LV_BEZIER_VAL_MAX]; same control points as lv_anim_path_*.
int32_t curve_step(Curve curve, int32_t t);

// A duration is a plain millisecond value, or one of these tokens resolved against the motion tokens at run time
// (so `fx::motion()` changes apply to the built-in tables as well).  DUR_DEFAULT: effect track "not given".
constexpr uint16_t DUR_DEFAULT = 0xFFFC;
constexpr uint16_t DUR_FAST = 0xFFFD;
constexpr uint16_t DUR_NORMAL = 0xFFFE;
constexpr uint16_t DUR_SLOW = 0xFFFF;

// ---- tracks ---------------------------------------------------------------------------------------------------

enum class TrackKind : uint8_t { PULSE, NUDGE, SHAKE, FLASH, GLOW, BLINK, IMAGE_PULSE, COUNT };

enum class Axis : uint8_t { DEFAULT, X, Y };  // DEFAULT = x
// Which style property a flash drives.  AUTO: text color of a label, recolor of an image, background otherwise.
enum class FlashProp : uint8_t { AUTO, BG_COLOR, TEXT_COLOR, IMAGE_RECOLOR };
enum class FlashProfile : uint8_t { DEFAULT, FADE_OUT, TRIANGLE };

constexpr uint16_t UNSET_U16 = 0xFFFF;
constexpr int16_t UNSET_I16 = INT16_MIN;
constexpr uint8_t UNSET_U8 = 0xFF;
constexpr int32_t UNSET_COLOR = -1;  // otherwise 0xRRGGBB

// One material moving one property.  Every motion is an offset that is zero before `delay` and again once `duration`
// has elapsed; fields apply to the kinds noted, the others ignore them.
struct Track {
  TrackKind kind;
  Curve curve = Curve::DEFAULT;       // all: shapes the track's own time (linear when unset)
  uint16_t delay = 0;                 // all: ms
  uint16_t duration = DUR_DEFAULT;    // all: ms or DUR_* (default: shake fast, flash/glow slow, others normal)
  uint16_t scale = UNSET_U16;         // pulse: peak scale in percent (110)
  int16_t x = UNSET_I16;              // nudge: peak offset in px (0)
  int16_t y = UNSET_I16;              // nudge
  uint16_t distance = UNSET_U16;      // shake: amplitude in px (2)
  uint8_t cycles = 0;                 // shake: back-and-forth count (2); 0 = not given
  Axis axis = Axis::DEFAULT;          // shake (x)
  FlashProp property = FlashProp::AUTO;  // flash
  int32_t color = UNSET_COLOR;        // flash: highlight 0xRRGGBB (white)
  uint8_t amount = UNSET_U8;          // flash: highlight strength in percent (100)
  FlashProfile profile = FlashProfile::DEFAULT;  // flash: fade_out (default) or triangle
  uint8_t opacity = UNSET_U8;         // glow: peak outline opacity in percent (50); blink: lowest opacity (30)
};

// ---- effects ---------------------------------------------------------------------------------------------

enum class Retrigger : uint8_t { DEFAULT, RESTART, IGNORE };
enum class WhenSuppressed : uint8_t { DEFAULT, SKIP, DEFER };
enum class EffectPolicy : uint8_t { DEFAULT, NORMAL, ALWAYS };  // ALWAYS ignores `suppress_when`
// What a `cooldown` is measured against: the last start of the same effect on the widget, or of any one-shot effect.
enum class CooldownScope : uint8_t { DEFAULT, EFFECT, WIDGET };

constexpr uint16_t REPEAT_INFINITE = 0xFFFF;  // a loop: "on" until stopped

struct EffectDef;
using EffectRef = const EffectDef *;

struct EffectDef {
  const Track *tracks;
  uint8_t track_count;
  uint8_t builtin = 0;                         // 0 = user effect, else 1..BUILTIN_COUNT: selects its preset
  Retrigger retrigger = Retrigger::DEFAULT;    // restart
  WhenSuppressed when_suppressed = WhenSuppressed::DEFAULT;  // skip
  EffectPolicy policy = EffectPolicy::DEFAULT;               // normal
  uint16_t repeat = 0;                         // 1, N or REPEAT_INFINITE; 0 = not given (1)
  uint16_t repeat_delay = UNSET_U16;           // ms between repeats (0)
  uint16_t cooldown = UNSET_U16;               // ms (0)
  uint16_t defer_timeout = UNSET_U16;          // ms (3000)
  const EffectDef *then = nullptr;             // played on the same target when a one-shot ends; not for loops
  // New fields go at the end: generated tables are designated initializers in declaration order.
  CooldownScope cooldown_scope = CooldownScope::DEFAULT;  // effect
  // `on_start:` hook, called with the target once per play of a one-shot, after the motion has started and the runner's
  // state is consistent (so it may call play / stop).  Not called for loops.  A captureless function.
  void (*on_start)(lv_obj_t *) = nullptr;
  // `on_end:` hook, called with the target when a one-shot has ended naturally after all its repeats, at the moment
  // its `then` would play and just before it (the runner is consistent, so it may call play / stop).  Not called on
  // stop(), replacement by another one-shot or widget deletion.  Not for loops.  A captureless function.
  void (*on_end)(lv_obj_t *) = nullptr;
};

// Global defaults for the options above (`lvgl_fx: effect_defaults:`); also the hook for C++ users:
// `fx::effect_defaults().cooldown = 500;`.  An effect's own value always wins.
struct EffectOptions {
  Retrigger retrigger = Retrigger::RESTART;
  WhenSuppressed when_suppressed = WhenSuppressed::SKIP;
  EffectPolicy policy = EffectPolicy::NORMAL;
  uint16_t repeat = 1;
  uint16_t repeat_delay = 0;
  uint16_t cooldown = 0;
  uint16_t defer_timeout = 3000;
  CooldownScope cooldown_scope = CooldownScope::EFFECT;  // DEFAULT is read as EFFECT
};
EffectOptions &effect_defaults();

// ---- built-in effects (builtins.cpp) --------------------------------------------------------------------------------

enum : uint8_t {
  BUILTIN_PULSE = 1,
  BUILTIN_SHAKE,
  BUILTIN_NUDGE_UP,
  BUILTIN_NUDGE_DOWN,
  BUILTIN_NUDGE_LEFT,
  BUILTIN_NUDGE_RIGHT,
  BUILTIN_FLASH,
  BUILTIN_GLOW,
  BUILTIN_BLINK,
  BUILTIN_LAST = BUILTIN_BLINK,
};
constexpr size_t BUILTIN_COUNT = BUILTIN_LAST;

extern const EffectDef PULSE_DEF, SHAKE_DEF, NUDGE_UP_DEF, NUDGE_DOWN_DEF, NUDGE_LEFT_DEF, NUDGE_RIGHT_DEF, FLASH_DEF,
    GLOW_DEF, BLINK_DEF;

inline constexpr EffectRef PULSE = &PULSE_DEF;
inline constexpr EffectRef SHAKE = &SHAKE_DEF;
inline constexpr EffectRef NUDGE_UP = &NUDGE_UP_DEF;
inline constexpr EffectRef NUDGE_DOWN = &NUDGE_DOWN_DEF;
inline constexpr EffectRef NUDGE_LEFT = &NUDGE_LEFT_DEF;
inline constexpr EffectRef NUDGE_RIGHT = &NUDGE_RIGHT_DEF;
inline constexpr EffectRef FLASH = &FLASH_DEF;
inline constexpr EffectRef GLOW = &GLOW_DEF;
inline constexpr EffectRef BLINK = &BLINK_DEF;

// Adjust the magnitudes of one built-in effect (`presets:`).  `adjust` is a const Track of the effect's own kind; the
// fields it sets win over the built-in values, the others stay.  nullptr restores the built-in values.  The table must
// outlive the program (a `static const` object).  Example: `static const fx::Track p = {.kind = fx::TrackKind::PULSE,
// .scale = 112}; fx::set_preset(fx::PULSE, &p);`.  Ignored for user effects.
void set_preset(EffectRef builtin, const Track *adjust);

// ---- frame math (tracks.cpp) ----------------------------------------------------------------------------------------

// Length of an effect in ms (the latest track end, at most 65535) and the PropBit mask its tracks drive.
uint32_t effect_length(const EffectDef &def);
uint8_t effect_props(const EffectDef &def);
// Highest outline opacity (0..255) any glow track reaches at intensity 1; 0 without a glow track.
uint8_t effect_glow_peak(const EffectDef &def);
// Property of the first flash track, preset-resolved (AUTO when there is none).
FlashProp effect_flash_property(const EffectDef &def);
// Add the offsets of every track at `time_ms` into the effect to `out`.  `intensity_q8` is the motion intensity in
// Q8 (256 = 1.0).  Integer math only, no allocation; zero before a track's delay and from its end on.
void effect_contribute(const EffectDef &def, uint32_t time_ms, uint32_t intensity_q8, Offsets &out);

}  // namespace lvgl_fx
}  // namespace esphome
