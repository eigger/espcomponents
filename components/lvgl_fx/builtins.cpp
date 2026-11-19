#include "lvgl_fx.h"

namespace esphome {
namespace lvgl_fx {

// Built-in per-property transitions.  Same const-table format that P4 codegen emits for user
// definitions, so the runtime has a single resolution path.  These are the last level of the fallback chain, so every
// field a property uses is set here (duration, curve, style, and the style parameters of the property).
namespace {

const PropTransition VISIBLE = {.duration = DUR_NORMAL, .delay = 0, .curve = Curve::EASE_OUT, .style = Style::FADE,
                                .hide_style = Style::FADE, .from = 90, .distance = 12};
const PropTransition OPACITY = {.duration = DUR_NORMAL, .delay = 0, .curve = Curve::LINEAR, .style = Style::NONE,
                                .hide_style = Style::NONE};
const PropTransition VALUE = {.duration = DUR_NORMAL, .delay = 0, .curve = Curve::EASE_OUT, .style = Style::NONE,
                              .hide_style = Style::NONE};
const PropTransition TEXT = {.duration = DUR_FAST, .delay = 0, .curve = Curve::LINEAR, .style = Style::FADE,
                             .hide_style = Style::FADE, .from = 85, .distance = 14, .format = "%.0f"};
const PropTransition COLOR = {.duration = DUR_NORMAL, .delay = 0, .curve = Curve::LINEAR, .style = Style::NONE,
                              .hide_style = Style::NONE};
const PropTransition POSITION = {.duration = DUR_NORMAL, .delay = 0, .curve = Curve::EASE_OUT, .style = Style::NONE,
                                 .hide_style = Style::NONE};
const PropTransition ROTATION = {.duration = DUR_NORMAL, .delay = 0, .curve = Curve::EASE_OUT, .style = Style::NONE,
                                 .hide_style = Style::NONE};

}  // namespace

// Order must match enum class Prop.
const TransitionDef BUILTIN_TRANSITIONS = {{&VISIBLE, &OPACITY, &VALUE, &TEXT, &COLOR, &POSITION, &ROTATION},
                                            nullptr};

// Built-in effects: one material each, every magnitude left unset so it resolves against the material's built-in
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
