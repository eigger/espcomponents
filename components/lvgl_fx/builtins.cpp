#include "lvgl_fx.h"

namespace esphome {
namespace lvgl_fx {

// Built-in per-property transitions (DESIGN.md 4.3).  Same const-table format that P4 codegen emits for user
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

}  // namespace lvgl_fx
}  // namespace esphome
