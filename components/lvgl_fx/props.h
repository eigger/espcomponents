#pragma once

// Property composition (DESIGN.md section 6).
//
// The widget's style is always written as  base value (+) offsets.  Only transitions change the base value.  Offsets
// are never stored: every contributor (effect, loop, transition-style entry) derives its offset from its own progress
// each time the property is composed, and every contributor ends at the identity offset.  Whatever order contributors
// finish in, the last write is the plain base value.

#include "esphome/core/defines.h"

#ifndef LV_CONF_H
#define LV_CONF_SKIP 1  // NOLINT
#endif

#include <cstdint>
#include <lvgl.h>

namespace esphome {
namespace lvgl_fx {

// Composed properties.  A slot only rewrites the bits it owns (7.5).
enum PropBit : uint8_t {
  PROP_OPA = 1 << 0,        // multiplied                    -> style opa
  PROP_SCALE = 1 << 1,      // multiplied                    -> transform_scale_x/y
  PROP_ROTATION = 1 << 2,   // added (0.1 degree)            -> transform_rotation
  PROP_COLOR = 1 << 3,      // mixed                         -> text color / image recolor / bg color
  PROP_OUTLINE = 1 << 4,    // max (base is 0)               -> outline_opa
  PROP_TRANSLATE = 1 << 5,  // summed (base is 0)            -> translate_x/y
};

// Which style property carries the "color" of a widget.
enum class ColorTarget : uint8_t { BG, TEXT, IMAGE };

// Sum of every contributor's offset on one target.  The default value is the identity for every merge rule, so an
// idle slot composes to exactly its base values.
struct Offsets {
  uint8_t opa = LV_OPA_COVER;           // multiplied
  uint16_t scale = LV_SCALE_NONE;       // multiplied (256 = 100%)
  int16_t rotation = 0;                 // added
  int16_t translate_x = 0;              // added
  int16_t translate_y = 0;              // added
  lv_color_t color = {0, 0, 0};         // mixed on top of the base color ...
  uint8_t color_amount = 0;             // ... by this much (0 = no contribution)
  uint8_t outline_opa = 0;              // max

  // Merge another contributor into this sum.  Contributors are commutative except for color, which blends in order.
  void merge(const Offsets &other);
};

// What a running instance contributes.  P3 adds the slide/zoom/roll style entries.
enum class InstanceKind : uint8_t {
  NONE = 0,
  FADE_IN,  // text fade style: opa offset = progress (0..255) -> 255
  EFFECT,   // one-shot or loop effect: progress = ms into the effect, state = PropBit mask its tracks drive
};

// One running piece of motion on a target: {definition, progress, state} (7.3).  An idle instance is all zero.
struct Instance {
  const void *def;     // definition the instance plays (effect / style entry); unused by FADE_IN
  uint16_t progress;   // kind-specific progress, written by the lv_anim callback
  InstanceKind kind;
  uint8_t state;       // kind-specific
};

struct EffectDef;
struct TransitionDef;

enum SlotFlag : uint8_t {
  SLOT_HIDING = 1 << 0,        // a hide transition is running; HIDDEN is set when it completes
  SLOT_LARGE_WARNED = 1 << 1,  // already warned about layer-rendering a large target (7.5)
  SLOT_LOOP_ON = 1 << 2,       // a loop effect is switched on (slot.loop.def)
  SLOT_LOOP_PAUSED = 1 << 3,   // the loop is on but at rest: policy, or not allowed to animate yet (5)
  SLOT_LOOP_FORCE = 1 << 4,    // the loop was started with force: policy never pauses it
  SLOT_UNTINTED = 1 << 5,      // image without recolor: a color contribution is the whole tint (see compose)
};

// Per-target state.  Created only when a transition or effect needs composition (7.3).  No default member
// initializers on purpose: the pool is zero-filled and `Slot{}` resets a slot, which keeps it trivially copyable.
struct Slot {
  lv_obj_t *obj;                  // target; nullptr = free slot
  uint16_t generation;            // visible generation: stale hide completions are ignored (6)
  uint8_t props;                  // PropBit mask of properties owned by fx
  uint8_t flags;                  // SlotFlag
  uint8_t base_opa;               // current base opacity (animated by transitions)
  uint8_t visible_opa;            // opacity the object has while shown: target of set_visible(true)
  uint16_t base_scale;            // 256 = 100%
  int16_t base_rotation;          // 0.1 degree
  lv_color_t base_color;          // current base color
  lv_color_t color_from;          // set_color transition endpoints (base_color is mixed between them)
  lv_color_t color_to;
  ColorTarget color_target;
  uint8_t base_outline_opa;       // outline opacity the widget had when fx took it over (glow adds on top)
  Instance one_shot, loop, style;  // one_shot keeps its last effect after it ended (cooldown)
  uint32_t last_play_tick;        // lv_tick of the last one-shot start (cooldown)
  const EffectDef *deferred;      // effect kept by `when_suppressed: defer` (one per target)
  uint32_t deferred_tick;         // lv_tick when it was kept (defer_timeout)
  const TransitionDef *opa_then;  // `then` of the running opacity/visibility transition (user_data: generation)
};

// Take ownership of a property: read the widget's current value as the base value the first time (6, ownership rule).
void own(Slot &slot, uint8_t props);
// Take ownership of the color property through a specific style property (a target has a single color channel).
// When the slot already carries color through another property: with `busy` (an effect or transition drives it) the
// call fails and returns false; otherwise the old property gets its base value back and the channel moves to `target`.
bool own_color(Slot &slot, ColorTarget target, bool busy);

// Recompute the owned properties in `mask` from base values and all contributors, and write them to the widget.
void compose(Slot &slot, uint8_t mask);

// Add every running instance's offset to `out`.  New contributor kinds hook in here (props.cpp).
void contribute(const Slot &slot, Offsets &out);

// Direct (non-composed) color access for widgets without a slot or when composition is bypassed.
ColorTarget color_target_of(lv_obj_t *obj);
lv_color_t read_color(lv_obj_t *obj, ColorTarget target);
void write_color(lv_obj_t *obj, ColorTarget target, lv_color_t color);

}  // namespace lvgl_fx
}  // namespace esphome
