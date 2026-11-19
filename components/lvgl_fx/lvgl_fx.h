#pragma once

// lvgl_fx: motion for ESPHome LVGL dashboards.  See DESIGN.md.
//
// Everything runs on the main loop thread (lv_timer_handler and ESPHome loop share it), so there is no locking.

#include "esphome/core/component.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#include <cstddef>
#include <cstdint>

#include "props.h"

namespace esphome {
namespace lvgl_fx {

// ---- curves ------------------------------------------------------------------------------------------------------

enum class Curve : uint8_t { LINEAR, EASE_IN, EASE_OUT, EASE_IN_OUT };

// LVGL path callback for a curve (used for lv_anim based motion).
lv_anim_path_cb_t curve_path(Curve curve);
// Integer curve evaluation for runners that cut sub-ranges out of one lv_anim (7.1).  t and the result are in
// [0, LV_BEZIER_VAL_MAX]; same control points as lv_anim_path_*.
int32_t curve_step(Curve curve, int32_t t);

// ---- transition definitions (const tables; P4 codegen emits these into flash) ---------------------------------------

enum class Prop : uint8_t { VISIBLE, OPACITY, VALUE, TEXT, COLOR, POSITION, ROTATION, COUNT };
constexpr size_t PROP_COUNT = static_cast<size_t>(Prop::COUNT);

// Transition styles (4.2.1).  P3 adds slide_*, zoom, roll, count.
enum class Style : uint8_t { NONE, FADE };

// A duration is a plain millisecond value, or one of these tokens resolved against the motion tokens at run time
// (so `fx::motion()` changes apply to the built-in tables as well).
constexpr uint16_t DUR_FAST = 0xFFFD;
constexpr uint16_t DUR_NORMAL = 0xFFFE;
constexpr uint16_t DUR_SLOW = 0xFFFF;

struct EffectDef;  // P2

// How one property moves to a new value.
struct PropTransition {
  uint16_t duration;  // ms or DUR_*; 0 = instant
  uint16_t delay;     // ms
  Curve curve;
  Style style;        // entry style; for VISIBLE the style used when showing
  Style hide_style;   // VISIBLE only: style used when hiding
  uint8_t from;       // style start, percent (text fade: opacity the new text starts at)
  int16_t distance;   // style travel in px (P3: slide, roll)
};

// A named transition: per-property entries (nullptr = fall back to the defaults) plus an optional follow-up effect.
// P4 emits `static const TransitionDef` objects and references them from C++ as TransitionRef.
struct TransitionDef {
  const PropTransition *props[PROP_COUNT];
  const EffectDef *then;  // P2: effect played on the same target when the transition completes (no-op in P1)
};
using TransitionRef = const TransitionDef *;

// Built-in per-property defaults (builtins.cpp).  Every entry is non-null.
extern const TransitionDef BUILTIN_TRANSITIONS;

// ---- call options ------------------------------------------------------------------------------------------------

struct Opt {
  TransitionRef transition = nullptr;  // named transition; nullptr = per-property default
  bool force = false;                  // ignore policy suppression (boot sequences)
};

// Motion tokens (4.1).  Runtime-changeable, e.g. `fx::motion().intensity = 0.6f;`.
struct Motion {
  uint16_t fast = 120;
  uint16_t normal = 200;
  uint16_t slow = 320;
  float intensity = 1.0f;  // scales effect size only, never a transition's target value
};
Motion &motion();

// ---- internals shared with the runner, styles and codegen (P2+) -----------------------------------------------------

namespace detail {

struct Core {
  Motion motion;
  bool enabled = true;
  bool reduced_motion = false;
  bool ready = false;
  bool (*suppress_fn)() = nullptr;  // `suppress_when`, evaluated at most once per main-loop iteration
  uint32_t loop_epoch = 1;          // bumped by LvglFx::loop()
  uint32_t suppress_epoch = 0;
  bool suppress_cached = false;
  bool slots_in_psram = false;
  bool overflow_warned = false;
  bool pool_failed = false;
  uint8_t capacity = 32;            // max_targets
  uint8_t used = 0;                 // high-water mark of slots in use
  Slot *slots = nullptr;
  Slot *last_hit = nullptr;
  TransitionRef defaults = nullptr;  // overrides BUILTIN_TRANSITIONS per property (P4 `transitions.defaults`)
};
Core &core();

// Slot lookup: linear search with a last-hit cache.  Returns nullptr when the target has no slot.
Slot *find_slot(lv_obj_t *obj);
// Find or create the slot of a target.  nullptr when the pool is full: callers then apply the final state instantly.
Slot *acquire_slot(lv_obj_t *obj);

// Policy, Transition column of the table in section 5.  `check_self_hidden` is false for set_visible, which changes
// that very flag.
bool transition_allowed(lv_obj_t *obj, bool check_self_hidden, const Opt &opt);

// Property entry of a transition: opt.transition, then the configured defaults, then the built-ins.
const PropTransition &resolve(TransitionRef transition, Prop prop);
uint32_t resolve_duration(uint16_t duration);

}  // namespace detail

// ---- C++ API (3.2) -------------------------------------------------------------------------------------------------

// Transitions: pass a value, the motion comes from the definition.  The final state is always exact, whatever the
// policy decides.
void set_visible(lv_obj_t *obj, bool shown, Opt opt = {});
void set_opacity(lv_obj_t *obj, lv_opa_t opa, Opt opt = {});
// bar/arc/slider: value; image: rotation in degrees.  NaN: instantly the minimum (image: 0).  Label counting is P3.
void set_value(lv_obj_t *obj, float value, Opt opt = {});
void set_text(lv_obj_t *obj, const char *text, Opt opt = {});
// label: text color, image: recolor, anything else: background color.
void set_color(lv_obj_t *obj, lv_color_t color, Opt opt = {});
void set_x(lv_obj_t *obj, int32_t x, Opt opt = {});
void set_y(lv_obj_t *obj, int32_t y, Opt opt = {});
// transform rotation of any widget.  Non-finite: instantly 0.  Mind transform_pivot.
void set_rotation(lv_obj_t *obj, float degrees, Opt opt = {});

// Stop all motion on a target and snap to the final state: transitions to their target value.
void stop(lv_obj_t *obj);

// Declare the UI ready.  Before this, transitions apply instantly (policy row 2).
void start();

// Policy switches (policy row 1).
void set_enabled(bool enabled);
void set_reduced_motion(bool reduced);

// ESPHome LVGL widgets (`id(widget)` of a compound widget type) are accepted wherever an lv_obj_t * is.
inline void set_visible(lvgl::LvCompound *w, bool shown, Opt opt = {}) { set_visible(w->obj, shown, opt); }
inline void set_opacity(lvgl::LvCompound *w, lv_opa_t opa, Opt opt = {}) { set_opacity(w->obj, opa, opt); }
inline void set_value(lvgl::LvCompound *w, float value, Opt opt = {}) { set_value(w->obj, value, opt); }
inline void set_text(lvgl::LvCompound *w, const char *text, Opt opt = {}) { set_text(w->obj, text, opt); }
inline void set_color(lvgl::LvCompound *w, lv_color_t color, Opt opt = {}) { set_color(w->obj, color, opt); }
inline void set_x(lvgl::LvCompound *w, int32_t x, Opt opt = {}) { set_x(w->obj, x, opt); }
inline void set_y(lvgl::LvCompound *w, int32_t y, Opt opt = {}) { set_y(w->obj, y, opt); }
inline void set_rotation(lvgl::LvCompound *w, float degrees, Opt opt = {}) { set_rotation(w->obj, degrees, opt); }
inline void stop(lvgl::LvCompound *w) { stop(w->obj); }

// ---- user channel (8.1) --------------------------------------------------------------------------------------------

struct TweenOpt {
  uint32_t duration = 0;               // ms; 0 = motion().normal
  Curve curve = Curve::EASE_OUT;
  uint32_t delay = 0;                  // ms
  void (*on_done)(void *var) = nullptr;  // runs when the tween ends, or right away when it is applied instantly
  lv_obj_t *target = nullptr;          // optional widget the tween belongs to: policy row 3 (hidden) applies to it
  bool force = false;
};

// Animate `setter(var, v)` from `from` to `to` with the policy applied.  Starting a tween on the same (var, setter)
// replaces the running one; the new tween starts from the caller's `from`, not where the old one stopped.  `var` must outlive the tween (an lv_obj_t * is cleaned
// up by LVGL when it is deleted).  Instant application calls setter(var, to) and then on_done synchronously, so on_done may re-enter.
void tween(void *var, lv_anim_exec_xcb_t setter, int32_t from, int32_t to, TweenOpt opt = {});
// Cancel a running tween without applying its end value.
void cancel_tween(void *var, lv_anim_exec_xcb_t setter);

// ---- component -----------------------------------------------------------------------------------------------------

class LvglFx : public Component {
 public:
  void set_motion(uint16_t fast, uint16_t normal, uint16_t slow, float intensity);
  void set_suppress_when(bool (*fn)()) { detail::core().suppress_fn = fn; }
  void set_max_targets(uint8_t count) { detail::core().capacity = count; }
  void set_slots_in_psram(bool psram) { detail::core().slots_in_psram = psram; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  // Configuration only; the slot pool is allocated lazily if an early on_boot call gets there first.
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace lvgl_fx
}  // namespace esphome
