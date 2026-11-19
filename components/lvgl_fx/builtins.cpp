#include "lvgl_fx.h"

namespace esphome {
namespace lvgl_fx {

// Built-in per-property transitions (DESIGN.md 4.2).  Same const-table format that P4 codegen emits for user
// definitions, so the runtime has a single resolution path.  Unspecified curves default to linear.
namespace {

//                                   duration    delay curve               style        hide_style   from distance
const PropTransition VISIBLE{DUR_NORMAL, 0, Curve::EASE_OUT, Style::FADE, Style::FADE, 0, 0};
const PropTransition OPACITY{DUR_NORMAL, 0, Curve::LINEAR, Style::NONE, Style::NONE, 0, 0};
const PropTransition VALUE{DUR_NORMAL, 0, Curve::EASE_OUT, Style::NONE, Style::NONE, 0, 0};
const PropTransition TEXT{DUR_FAST, 0, Curve::LINEAR, Style::FADE, Style::FADE, 85, 0};
const PropTransition COLOR{DUR_NORMAL, 0, Curve::LINEAR, Style::NONE, Style::NONE, 0, 0};
const PropTransition POSITION{DUR_NORMAL, 0, Curve::EASE_OUT, Style::NONE, Style::NONE, 0, 0};
const PropTransition ROTATION{DUR_NORMAL, 0, Curve::EASE_OUT, Style::NONE, Style::NONE, 0, 0};

}  // namespace

// Order must match enum class Prop.
const TransitionDef BUILTIN_TRANSITIONS = {{&VISIBLE, &OPACITY, &VALUE, &TEXT, &COLOR, &POSITION, &ROTATION},
                                            nullptr};

// Built-in effects (4.3): one material each, every magnitude left unset so it resolves against the material's built-in
// value and the effect's preset (set_preset).  The nudges move 4 px, which the design does not specify.
namespace {

constexpr int16_t NUDGE_PX = 4;

const Track PULSE_TRACKS[] = {{.kind = TrackKind::PULSE}};
const Track SHAKE_TRACKS[] = {{.kind = TrackKind::SHAKE}};
const Track NUDGE_UP_TRACKS[] = {{.kind = TrackKind::NUDGE, .y = -NUDGE_PX}};
const Track NUDGE_DOWN_TRACKS[] = {{.kind = TrackKind::NUDGE, .y = NUDGE_PX}};
const Track NUDGE_LEFT_TRACKS[] = {{.kind = TrackKind::NUDGE, .x = -NUDGE_PX}};
const Track NUDGE_RIGHT_TRACKS[] = {{.kind = TrackKind::NUDGE, .x = NUDGE_PX}};
const Track FLASH_TRACKS[] = {{.kind = TrackKind::FLASH}};
const Track GLOW_TRACKS[] = {{.kind = TrackKind::GLOW}};
const Track BLINK_TRACKS[] = {{.kind = TrackKind::BLINK}};

}  // namespace

const EffectDef PULSE_DEF = {.tracks = PULSE_TRACKS, .track_count = 1, .builtin = BUILTIN_PULSE};
const EffectDef SHAKE_DEF = {.tracks = SHAKE_TRACKS, .track_count = 1, .builtin = BUILTIN_SHAKE};
const EffectDef NUDGE_UP_DEF = {.tracks = NUDGE_UP_TRACKS, .track_count = 1, .builtin = BUILTIN_NUDGE_UP};
const EffectDef NUDGE_DOWN_DEF = {.tracks = NUDGE_DOWN_TRACKS, .track_count = 1, .builtin = BUILTIN_NUDGE_DOWN};
const EffectDef NUDGE_LEFT_DEF = {.tracks = NUDGE_LEFT_TRACKS, .track_count = 1, .builtin = BUILTIN_NUDGE_LEFT};
const EffectDef NUDGE_RIGHT_DEF = {.tracks = NUDGE_RIGHT_TRACKS, .track_count = 1, .builtin = BUILTIN_NUDGE_RIGHT};
const EffectDef FLASH_DEF = {.tracks = FLASH_TRACKS, .track_count = 1, .builtin = BUILTIN_FLASH};
const EffectDef GLOW_DEF = {.tracks = GLOW_TRACKS, .track_count = 1, .builtin = BUILTIN_GLOW};
const EffectDef BLINK_DEF = {.tracks = BLINK_TRACKS, .track_count = 1, .builtin = BUILTIN_BLINK};

}  // namespace lvgl_fx
}  // namespace esphome
