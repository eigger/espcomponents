#pragma once

// Property composition.
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

// Composed properties.  A slot only rewrites the bits it owns.
enum PropBit : uint8_t {
  PROP_OPA = 1 << 0,        // multiplied                    -> style opa
  PROP_SCALE = 1 << 1,      // multiplied                    -> transform_scale_x/y
  PROP_ROTATION = 1 << 2,   // added (0.1 degree)            -> transform_rotation
  PROP_COLOR = 1 << 3,      // mixed                         -> text color / image recolor / bg color
  PROP_OUTLINE = 1 << 4,    // max (base is 0)               -> outline_opa
  PROP_IMAGE_OPA = 1 << 6,   // additive image opacity pulse
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
  uint8_t image_opa_add = 0;           // added to the image opacity base
  uint8_t outline_opa = 0;              // max

  // Merge another contributor into this sum.  Contributors are commutative except for color, which blends in order.
  void merge(const Offsets &other);
};

// What a running instance contributes.
enum class InstanceKind : uint8_t {
  NONE = 0,
  EFFECT,      // one-shot or loop effect: progress = ms into the effect, state = PropBit mask its tracks drive
  TEXT_ENTER,  // text fade / roll: progress 0..255 = how far the new text has arrived; arg.a = signed travel in px
               // (0 = fade only), arg.b = starting opacity 0..255.  opa offset: arg.b -> 255, translate_y: arg.a -> 0
  COUNT,       // label count: moves no property (the lv_anim callback writes the text); format/state: see Instance
  SLIDE_UP,    // visible styles: offsets derived from the slot's base opacity, which the visible transition
  SLIDE_DOWN,  //   animates.  state = the opacity the target has while shown (offset 0), arg.a = distance in px
  SLIDE_LEFT,  //   (zoom: the starting scale in Q8).  The offset is zero at that opacity and full at opacity 0.
  SLIDE_RIGHT,
  ZOOM,
};

// One running piece of motion on a target.  An idle instance is all zero.  The first member is a union so that
// styles can keep their resolved parameters in the instance itself (the definitions hold unresolved "unset" fields).
struct Instance {
  union {
    const void *def;                // EFFECT: the EffectDef it plays
    const char *format;             // COUNT: printf format of the number (nullptr = "%.0f")
    struct {
      int16_t a, b;
    } arg;                          // style kinds: resolved parameters (see InstanceKind)
  };
  uint16_t progress;                // kind-specific progress, written by the lv_anim callback
  InstanceKind kind;
  uint8_t state;                    // EFFECT: PropBit mask; COUNT: decimals of the fixed-point lv_anim values;
                                    // SLIDE_*/ZOOM: shown opacity
};

struct EffectDef;
struct TransitionDef;

enum SlotFlag : uint8_t {
  SLOT_HIDING = 1 << 0,        // a hide transition is running; HIDDEN is set when it completes
  SLOT_LARGE_WARNED = 1 << 1,  // already warned about layer-rendering a large target
  SLOT_LOOP_ON = 1 << 2,       // a loop effect is switched on (slot.loop.def)
  SLOT_LOOP_PAUSED = 1 << 3,   // the loop is on but at rest: policy, or not allowed to animate yet
  SLOT_LOOP_FORCE = 1 << 4,    // the loop was started with force: policy never pauses it
  SLOT_UNTINTED = 1 << 5,      // image without recolor: a color contribution is the whole tint (see compose)
};

// Per-target state.  Created only when a transition or effect needs composition.  No default member
// initializers on purpose: the pool is zero-filled and `Slot{}` resets a slot, which keeps it trivially copyable.
struct Slot {
  lv_obj_t *obj;                  // target; nullptr = free slot
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
  uint8_t base_image_opa;         // image opacity without effect offsets
  uint8_t base_outline_opa;       // outline opacity the widget had when fx took it over (glow adds on top)
  Instance one_shot, loop;        // effects; one_shot keeps its last effect after it ended (cooldown)
  Instance style;                 // text style entry (fade / roll / count)
  Instance vis;                   // visible style entry/exit (slide / zoom), alive while a visible transition runs
  uint32_t last_play_tick;        // lv_tick of the last one-shot start (cooldown)
  const EffectDef *deferred;      // effect kept by `when_suppressed: defer` (one per target)
  uint32_t deferred_tick;         // lv_tick when it was kept (defer_timeout)
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
// PropBit mask an instance moves (what to compose when it starts, changes or ends).
uint8_t instance_props(const Instance &instance);

// Direct (non-composed) color access for widgets without a slot or when composition is bypassed.
ColorTarget color_target_of(lv_obj_t *obj);
lv_color_t read_color(lv_obj_t *obj, ColorTarget target);
void write_color(lv_obj_t *obj, ColorTarget target, lv_color_t color);

}  // namespace lvgl_fx
}  // namespace esphome
