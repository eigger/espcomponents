#pragma once

// lvgl_fx: motion for ESPHome LVGL dashboards.  See README.md.
//
// Everything runs on the main loop thread (lv_timer_handler and ESPHome loop share it), so there is no locking.

#include "esphome/core/component.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#include <cstddef>
#include <cstdint>

#include "effects.h"
#include "props.h"

namespace esphome {
namespace lvgl_fx {

// ---- transition definitions (const tables; P4 codegen emits these into flash) ---------------------------------------

enum class Prop : uint8_t { VISIBLE, OPACITY, VALUE, TEXT, COLOR, POSITION, ROTATION, COUNT };
constexpr size_t PROP_COUNT = static_cast<size_t>(Prop::COUNT);

// Transition styles.  Which style applies where: VISIBLE fade / slide_* / zoom / none, TEXT fade / roll /
// count / none; everything else is ignored for a property (a text entry asking for slide_* behaves as fade).
enum class Style : uint8_t { DEFAULT, NONE, FADE, SLIDE_UP, SLIDE_DOWN, SLIDE_LEFT, SLIDE_RIGHT, ZOOM, ROLL, COUNT };

// How one property moves to a new value.  Every field has an "unset" default: a definition only lists what it says
// (designated initializers, in declaration order), and each unset field falls back, at run time, to the same property
// in the configured defaults and then to the built-in table (detail::resolve).  P4 codegen can therefore emit
//
//   static const fx::PropTransition gear = {.duration = 140, .style = fx::Style::ROLL, .distance = 14};
//
// without knowing any default.
struct PropTransition {
  uint16_t duration = DUR_DEFAULT;      // ms or DUR_*; 0 = instant
  uint16_t delay = UNSET_U16;           // ms (0)
  Curve curve = Curve::DEFAULT;
  Style style = Style::DEFAULT;         // entry style; for VISIBLE the style used when showing
  Style hide_style = Style::DEFAULT;    // VISIBLE: style when hiding.  Unset: the same as `style` when this entry
                                        // sets `style`, otherwise the next level (defaults, built-ins) decides
  uint8_t from = UNSET_U8;              // percent.  text fade/roll: opacity the new text starts at (85);
                                        // zoom: scale the target starts at / goes to when hiding (90)
  uint16_t distance = UNSET_U16;        // px.  slide_*: travel (12); roll: travel (14)
  const char *format = nullptr;         // label numbers: printf format with one %f conversion ("%.0f"); count only
                                        // counts through %f formats (set_value formats anything once)
};

// A named transition: per-property entries (nullptr = fall back to the defaults) plus an optional follow-up effect.
// P4 emits `static const TransitionDef` objects and references them from C++ as TransitionRef.
struct TransitionDef {
  const PropTransition *props[PROP_COUNT];
  const EffectDef *then;  // effect played on the same target when the transition completes (not on stop())
  EffectPolicy policy = EffectPolicy::NORMAL;  // ALWAYS ignores suppress_when only
};
using TransitionRef = const TransitionDef *;

// Built-in per-property defaults (builtins.cpp).  Every entry is non-null.
extern const TransitionDef BUILTIN_TRANSITIONS;

// Statically allocated widget/property binding. The getter is evaluated after widgets are created.
struct Binding {
  lv_obj_t *(*get)();
  Prop prop;
  TransitionRef transition;
  lv_obj_t *obj = nullptr;
  Binding *next = nullptr;
};

// ---- call options ------------------------------------------------------------------------------------------------

struct Opt {
  TransitionRef transition = nullptr;  // named transition; nullptr = per-property default
  bool force = false;                  // ignore policy suppression (boot sequences)
  bool always = false;                 // ignore suppress_when only
  bool instant = false;                // cancel this property and apply immediately, without then
};

// Motion tokens.  Runtime-changeable, e.g. `fx::motion().intensity = 0.6f;`.
struct Motion {
  uint16_t fast = 120;
  uint16_t normal = 200;
  uint16_t slow = 320;
  float intensity = 1.0f;  // scales effect size only, never a transition's target value
};
Motion &motion();

// Select a property-specific binding only when the caller did not supply a transition.
Opt select_transition(lv_obj_t *obj, Prop prop, Opt opt);

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
  EffectOptions effect_defaults;     // `effect_defaults:`
  const Track *presets[BUILTIN_COUNT] = {};  // `presets:` per built-in effect, indexed by EffectDef::builtin - 1
  uint32_t intensity_q8 = LV_SCALE_NONE;     // motion().intensity as sampled when the last effect started
  uint8_t watching = 0;             // slots with a deferred effect or a loop on: LvglFx::loop() has work
  Component *owner = nullptr;       // the component, to wake its loop()
};
Core &core();

// Slot lookup: linear search with a last-hit cache.  Returns nullptr when the target has no slot.
Slot *find_slot(lv_obj_t *obj);
// Find or create the slot of a target.  nullptr when the pool is full: callers then apply the final state instantly.
Slot *acquire_slot(lv_obj_t *obj);

// Policy building blocks: the hidden test walks the ancestors; `suppress_when` is cached per main-loop iteration.
bool target_hidden(lv_obj_t *obj, bool check_self);
bool suppressed();

// Policy, Transition column of the table in section 5.  `check_self_hidden` is false for set_visible, which changes
// that very flag.
bool transition_allowed(lv_obj_t *obj, bool check_self_hidden, const Opt &opt);

// A property entry with every field resolved: opt.transition, then the configured defaults, then the built-ins, field
// by field.  Durations are in ms.
struct ResolvedTransition {
  uint32_t duration;
  uint16_t delay;
  Curve curve;
  Style style;
  Style hide_style;
  uint8_t from;       // percent
  uint16_t distance;  // px
  const char *format;
};
ResolvedTransition resolve(TransitionRef transition, Prop prop);
uint32_t resolve_duration(uint16_t duration);

// ---- animation plumbing (lvgl_fx.cpp), shared with styles.cpp ----
// Key rules for lv_anim: (var, exec_cb) identifies an animation and lv_anim_start replaces a running one with the same
// pair.  Everything slot based uses var = slot with one exec_cb per property or instance, so a single
// lv_anim_delete(slot, nullptr) on target deletion clears it all.  False when LVGL could not allocate the animation.
bool start_anim(void *var, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t duration, uint32_t delay,
                Curve curve, lv_anim_completed_cb_t done = nullptr, void *user_data = nullptr,
                bool early_apply = true, lv_anim_deleted_cb_t deleted = nullptr);  // false: nothing is applied (and nothing replaced) before `delay` is over
// `then` of a call as an anim user_data (nullptr = nothing to play), and its two ways of running.
void *then_of(const Opt &opt);
void instant_then(lv_obj_t *obj, const Opt &opt);  // transition applied instantly: it completes at once
void slot_done(lv_anim_t *anim);                   // anim completed: play the `then` in user_data on the slot's target
// Stop one animation and run it to its end state; completion callbacks that complete state run, `then` does not.
void snap(void *var, lv_anim_exec_xcb_t exec);
void warn_once(uint8_t id, const char *what);

// ---- transition styles (styles.cpp) ----
// Visible slide/zoom: an offset instance (Slot::vis) beside the opacity transition.  `in_flight`: a visible transition
// is already running (a reversal) - it keeps the style it started with, so the motion continues instead of jumping.
void begin_visible_style(Slot *slot, Style style, const ResolvedTransition &t, bool in_flight);
// End the visible style's offsets (offsets 0) and compose them.
void drop_visible_style(Slot *slot);
// Label numbers (3.2 set_value on a label) and the text styles' animation.
void set_label_value(lv_obj_t *obj, float value, const Opt &opt);
// stop(): run the text style to its end state and drop it.
void stop_text(Slot *slot);

// ---- effect runner (runner.cpp) ----
// A slot is "watched" while LvglFx::loop() has to look at it: a deferred effect, or a loop that is on.
inline bool slot_watched(const Slot &slot) { return slot.deferred != nullptr || (slot.flags & SLOT_LOOP_ON) != 0; }
// True while a set_color transition is animating the slot's base color.
bool color_transition_running(Slot &slot);
// Stop every effect on the slot: the one-shot and the loop end at rest (offsets 0), a deferred effect is dropped.
void stop_effects(Slot &slot);
// Slot teardown: bookkeeping only, the animations are deleted by the caller (they all use the slot as var).
void release_effects(Slot &slot);
// `then` of a finished transition or effect: play it on the same target with default policy.
void play_then(lv_obj_t *obj, EffectRef then);
// One pass over deferred effects and loops (LvglFx::loop()).
void service_effects();

}  // namespace detail

// ---- C++ API -------------------------------------------------------------------------------------------------

// Transitions: pass a value, the motion comes from the definition.  The final state is always exact, whatever the
// policy decides.
void set_visible(lv_obj_t *obj, bool shown, Opt opt = {});
void set_opacity(lv_obj_t *obj, lv_opa_t opa, Opt opt = {});
// bar/arc/slider: value; image: rotation in degrees; label: the number, formatted with the text transition's `format`
// and applied like set_text, or counted from the previous number with the `count` style.  NaN: instantly the minimum
// (image: 0, label: "--").
void set_value(lv_obj_t *obj, float value, Opt opt = {});
void set_text(lv_obj_t *obj, const char *text, Opt opt = {});
// label: text color, image: recolor, anything else: background color.
void set_color(lv_obj_t *obj, lv_color_t color, Opt opt = {});
void set_x(lv_obj_t *obj, int32_t x, Opt opt = {});
void set_y(lv_obj_t *obj, int32_t y, Opt opt = {});
// transform rotation of any widget.  Non-finite: instantly 0.  Mind transform_pivot.
void set_rotation(lv_obj_t *obj, float degrees, Opt opt = {});

// Effects: pass an effect.  The only call-site override is `opt.force`; `opt.transition` is ignored.
void play(lv_obj_t *obj, EffectRef effect, Opt opt = {});

// Stop all motion on a target and snap to the final state: transitions to their target value, effects to rest.
void stop(lv_obj_t *obj);
// Stop one effect (running one-shot, loop that is on, or deferred) without touching anything else.
void stop(lv_obj_t *obj, EffectRef effect);

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
inline void play(lvgl::LvCompound *w, EffectRef effect, Opt opt = {}) { play(w->obj, effect, opt); }
inline void stop(lvgl::LvCompound *w) { stop(w->obj); }
inline void stop(lvgl::LvCompound *w, EffectRef effect) { stop(w->obj, effect); }

void set_image_opa(lv_obj_t *obj, uint8_t opacity);
inline void set_image_opa(lvgl::LvCompound *w, uint8_t opacity) { set_image_opa(w->obj, opacity); }

// ---- user channel --------------------------------------------------------------------------------------------

struct TweenOpt {
  uint32_t duration = 0;               // ms; 0 = motion().normal
  Curve curve = Curve::EASE_OUT;
  uint32_t delay = 0;                  // ms
  void (*on_done)(void *var) = nullptr;  // runs when the tween ends, or right away when it is applied instantly
  lv_obj_t *target = nullptr;          // optional widget the tween belongs to: policy row 3 (hidden) applies to it
  bool force = false;
  bool always = false;
  EffectRef completion_effect = nullptr;
  lv_obj_t *completion_target = nullptr;
};

// Animate `setter(var, v)` from `from` to `to` with the policy applied.  Starting a tween on the same (var, setter)
// replaces the running one; the new tween starts from the caller's `from`, not where the old one stopped.  `var` must
// outlive the tween (an lv_obj_t * is cleaned up by LVGL when it is deleted).  Instant application calls
// setter(var, to) and then on_done synchronously, so on_done may re-enter.
void tween(void *var, lv_anim_exec_xcb_t setter, int32_t from, int32_t to, TweenOpt opt = {});
// Cancel a running tween without applying its end value.
void cancel_tween(void *var, lv_anim_exec_xcb_t setter);
// Disarm pending cross-widget completion effects while leaving their tweens running.
void cancel_tween_effect(lv_obj_t *target);

// ---- component -----------------------------------------------------------------------------------------------------

class LvglFx : public Component {
 public:
  void set_motion(uint16_t fast, uint16_t normal, uint16_t slow, float intensity);
  void set_suppress_when(bool (*fn)()) { detail::core().suppress_fn = fn; }
  void set_max_targets(uint8_t count) { detail::core().capacity = count; }
  void set_slots_in_psram(bool psram) { detail::core().slots_in_psram = psram; }
  // `transitions: defaults:` (a const table emitted by the codegen, never freed).
  void set_transition_defaults(TransitionRef defaults) { detail::core().defaults = defaults; }
  void add_binding(Binding *binding);

  void setup() override;
  void loop() override;
  void dump_config() override;
  // Configuration only; the slot pool is allocated lazily if an early on_boot call gets there first.
  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace lvgl_fx
}  // namespace esphome
