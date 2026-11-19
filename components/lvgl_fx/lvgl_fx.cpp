#include "lvgl_fx.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lvgl_fx {

static const char *const TAG = "lvgl_fx";

#if INTPTR_MAX == INT32_MAX
// DESIGN.md 7.3 budgets ~52 bytes per target on the 32-bit targets; keep it from creeping up unnoticed.
static_assert(sizeof(Slot) <= 52, "lvgl_fx::Slot grew past its size budget");
#endif

// ---- state -------------------------------------------------------------------------------------------------------

namespace detail {

// Constant-initialized: no guard variable on access.
static Core g_core;
Core &core() { return g_core; }

// Allocated once (7.3), never per frame.  PSRAM first when asked for, with the allocator's internal fallback.
static bool ensure_pool(Core &c) {
  if (c.slots != nullptr) return true;
  if (c.pool_failed) return false;  // already logged once; callers fall back to instant application
  RAMAllocator<Slot> allocator(c.slots_in_psram ? RAMAllocator<Slot>::NONE : RAMAllocator<Slot>::ALLOC_INTERNAL);
  c.slots = allocator.allocate(c.capacity);
  if (c.slots == nullptr) {
    c.pool_failed = true;
    ESP_LOGE(TAG, "Cannot allocate %u target slots (%u bytes)", c.capacity,
             static_cast<unsigned>(c.capacity * sizeof(Slot)));
    return false;
  }
  std::memset(c.slots, 0, sizeof(Slot) * c.capacity);  // all-zero == free slot, idle instances, no owned props
  return true;
}

Slot *find_slot(lv_obj_t *obj) {
  Core &c = g_core;
  if (c.last_hit != nullptr && c.last_hit->obj == obj) return c.last_hit;
  for (uint8_t i = 0; i < c.used; i++) {
    if (c.slots[i].obj == obj) return c.last_hit = &c.slots[i];
  }
  return nullptr;
}

// Frees the slot with its target.  lv_obj_delete only removes animations whose var is the object itself; ours use the
// slot as var, so they must go here before the slot can be reused.
static void on_target_deleted(lv_event_t *e) {
  Core &c = g_core;
  auto *slot = static_cast<Slot *>(lv_event_get_user_data(e));
  // A child's DELETE can bubble up to a handler registered on its parent: only react to our own target.
  if (slot->obj == nullptr || lv_event_get_target(e) != slot->obj) return;
  lv_anim_delete(slot, nullptr);
  if (c.last_hit == slot) c.last_hit = nullptr;
  *slot = Slot{};
  while (c.used > 0 && c.slots[c.used - 1].obj == nullptr) c.used--;
}

Slot *acquire_slot(lv_obj_t *obj) {
  if (Slot *existing = find_slot(obj)) return existing;
  Core &c = g_core;
  if (!ensure_pool(c)) return nullptr;
  Slot *slot = nullptr;
  for (uint8_t i = 0; i < c.used; i++) {
    if (c.slots[i].obj == nullptr) {
      slot = &c.slots[i];
      break;
    }
  }
  if (slot == nullptr && c.used < c.capacity) slot = &c.slots[c.used++];
  if (slot == nullptr) {
    if (!c.overflow_warned) {
      c.overflow_warned = true;
      ESP_LOGW(TAG, "All %u target slots are in use (max_targets): motion on further targets is applied instantly",
               c.capacity);
    }
    return nullptr;
  }
  *slot = Slot{};
  slot->obj = obj;
  lv_obj_add_event_cb(obj, on_target_deleted, LV_EVENT_DELETE, slot);
  return c.last_hit = slot;
}

// ---- policy ------------------------------------------------------------------------------------------------------

static bool target_hidden(lv_obj_t *obj, bool check_self) {
  for (lv_obj_t *o = check_self ? obj : lv_obj_get_parent(obj); o != nullptr; o = lv_obj_get_parent(o)) {
    if (lv_obj_is_hidden(o)) return true;
  }
  return false;
}

// `suppress_when` runs at most once per main-loop iteration (LvglFx::loop() bumps the epoch).
static bool suppressed() {
  Core &c = g_core;
  if (c.suppress_fn == nullptr) return false;
  if (c.suppress_epoch != c.loop_epoch) {
    c.suppress_epoch = c.loop_epoch;
    c.suppress_cached = c.suppress_fn();
  }
  return c.suppress_cached;
}

// Rows 1-4 of the policy table, cheapest first so the lambda only runs when everything else says "animate" (7.4).
bool transition_allowed(lv_obj_t *obj, bool check_self_hidden, const Opt &opt) {
  const Core &c = g_core;
  if (opt.force) return true;
  if (!c.enabled || c.reduced_motion) return false;
  if (!c.ready) return false;
  if (obj != nullptr && target_hidden(obj, check_self_hidden)) return false;
  return !suppressed();
}

uint32_t resolve_duration(uint16_t duration) {
  const Motion &m = g_core.motion;
  switch (duration) {
    case DUR_FAST:
      return m.fast;
    case DUR_NORMAL:
      return m.normal;
    case DUR_SLOW:
      return m.slow;
    default:
      return duration;
  }
}

const PropTransition &resolve(TransitionRef transition, Prop prop) {
  const size_t i = static_cast<size_t>(prop);
  if (transition != nullptr && transition->props[i] != nullptr) return *transition->props[i];
  const TransitionDef *defaults = g_core.defaults;
  if (defaults != nullptr && defaults->props[i] != nullptr) return *defaults->props[i];
  return *BUILTIN_TRANSITIONS.props[i];
}

}  // namespace detail

using detail::acquire_slot;
using detail::find_slot;
using detail::resolve;
using detail::resolve_duration;
using detail::transition_allowed;

Motion &motion() { return detail::g_core.motion; }

// ---- curves ------------------------------------------------------------------------------------------------------

lv_anim_path_cb_t curve_path(Curve curve) {
  switch (curve) {
    case Curve::EASE_IN:
      return lv_anim_path_ease_in;
    case Curve::EASE_OUT:
      return lv_anim_path_ease_out;
    case Curve::EASE_IN_OUT:
      return lv_anim_path_ease_in_out;
    case Curve::LINEAR:
      break;
  }
  return lv_anim_path_linear;
}

int32_t curve_step(Curve curve, int32_t t) {
  switch (curve) {
    case Curve::EASE_IN:
      return lv_cubic_bezier(t, LV_BEZIER_VAL_FLOAT(0.42), 0, LV_BEZIER_VAL_FLOAT(1), LV_BEZIER_VAL_FLOAT(1));
    case Curve::EASE_OUT:
      return lv_cubic_bezier(t, 0, 0, LV_BEZIER_VAL_FLOAT(0.58), LV_BEZIER_VAL_FLOAT(1));
    case Curve::EASE_IN_OUT:
      return lv_cubic_bezier(t, LV_BEZIER_VAL_FLOAT(0.42), 0, LV_BEZIER_VAL_FLOAT(0.58), LV_BEZIER_VAL_FLOAT(1));
    case Curve::LINEAR:
      break;
  }
  return t;
}

// ---- animation plumbing ------------------------------------------------------------------------------------------

// Key rules for lv_anim: (var, exec_cb) identifies an animation.  lv_anim_start replaces a running animation with the
// same pair, so retargeting needs no explicit delete.  Slot-based motion uses var = slot, one exec_cb per property;
// slot-less motion (value, x, y) uses var = the widget.  Both are unique per property; neither can collide with a
// user tween, whose exec_cb is the user's.
static void start_anim(void *var, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t duration,
                       uint32_t delay, Curve curve, lv_anim_completed_cb_t done = nullptr, void *user_data = nullptr) {
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, var);
  lv_anim_set_exec_cb(&anim, exec);
  lv_anim_set_values(&anim, from, to);
  lv_anim_set_duration(&anim, duration);
  lv_anim_set_delay(&anim, delay);
  lv_anim_set_path_cb(&anim, curve_path(curve));
  if (done != nullptr) lv_anim_set_completed_cb(&anim, done);
  lv_anim_set_user_data(&anim, user_data);
  lv_anim_start(&anim);
}

// Slot-less transitions finish through this hook.  Used for `then` (DESIGN.md 4.2): the effect is played on the same
// target.  Effects arrive in P2; until then there is nothing to play.
static void on_transition_done(lv_obj_t *obj, TransitionRef transition) {
  (void) obj;
  (void) transition;
  // P2: if (transition->then != nullptr) play(obj, *transition->then, ...);
}

static void slotless_done(lv_anim_t *anim) {
  on_transition_done(static_cast<lv_obj_t *>(anim->var), static_cast<TransitionRef>(anim->user_data));
}

// Stop one animation and run it to its end state.  Completion callbacks that complete *state* (hide, text fade) run;
// the `then` hook does not, since stop() should not start new motion.
static void snap(void *var, lv_anim_exec_xcb_t exec) {
  lv_anim_t *running = lv_anim_get(var, exec);
  if (running == nullptr) return;
  lv_anim_t copy = *running;
  lv_anim_delete(var, exec);
  exec(var, copy.end_value);
  if (copy.completed_cb != nullptr && copy.completed_cb != slotless_done) copy.completed_cb(&copy);
}

static void warn_once(uint8_t id, const char *what) {
  static uint8_t warned = 0;
  if (warned & (1u << id)) return;
  warned |= static_cast<uint8_t>(1u << id);
  ESP_LOGW(TAG, "%s", what);
}

// Slot-less transition of an integer property of the widget itself (value, x, y): no base/offset composition is
// needed, and the widget's own getter is the base value, so external changes cannot desynchronize it (6).
static void move_slotless(lv_obj_t *obj, lv_anim_exec_xcb_t exec, Prop prop, int32_t from, int32_t to,
                          int32_t final_value, bool instant, const Opt &opt) {
  const PropTransition &t = resolve(opt.transition, prop);
  const uint32_t duration = resolve_duration(t.duration);
  if (instant || duration == 0 || from == to || !transition_allowed(obj, true, opt)) {
    lv_anim_delete(obj, exec);
    exec(obj, final_value);
    return;
  }
  void *then = (opt.transition != nullptr && opt.transition->then != nullptr)
                   ? const_cast<TransitionDef *>(opt.transition)
                   : nullptr;
  start_anim(obj, exec, from, to, duration, t.delay, t.curve, then != nullptr ? slotless_done : nullptr, then);
}

// ---- opacity / visibility ----------------------------------------------------------------------------------------

static void exec_opa(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  slot->base_opa = static_cast<uint8_t>(std::clamp<int32_t>(v, 0, LV_OPA_COVER));
  compose(*slot, PROP_OPA);
}

// The hide transition reached 0: now the target really goes away.
static void finish_hide(Slot *slot) {
  slot->flags &= static_cast<uint8_t>(~SLOT_HIDING);
  // Restore the shown opacity before hiding (an external un-hide then shows it as set); the next set_visible(true)
  // fades in from 0.
  slot->base_opa = slot->visible_opa;
  compose(*slot, PROP_OPA);
  lv_obj_set_hidden(slot->obj, true);
  // P3: reset slide/zoom entry offsets here; the target is invisible, so the jump back to 0 cannot be seen (6).
}

static void hide_done(lv_anim_t *anim) {
  auto *slot = static_cast<Slot *>(anim->var);
  // Generation: only the hide that is still current may complete (reversal or retarget bumped it otherwise).
  if (static_cast<uint16_t>(reinterpret_cast<uintptr_t>(anim->user_data)) != slot->generation) return;
  if (!(slot->flags & SLOT_HIDING)) return;
  finish_hide(slot);
}

static void start_opa(Slot *slot, int32_t from, int32_t to, const PropTransition &t, uint32_t duration, bool hide) {
  start_anim(slot, exec_opa, from, to, duration, t.delay, t.curve, hide ? hide_done : nullptr,
             hide ? reinterpret_cast<void *>(static_cast<uintptr_t>(slot->generation)) : nullptr);
}

void set_visible(lv_obj_t *obj, bool shown, Opt opt) {
  if (obj == nullptr) return;
  const bool hidden = lv_obj_is_hidden(obj);
  Slot *slot = find_slot(obj);
  const bool hiding = slot != nullptr && (slot->flags & SLOT_HIDING);
  // Already there (or on the way): nothing to do, and a running transition keeps going (7.4).
  if (shown ? (!hidden && !hiding) : (hidden || hiding)) return;

  if (slot == nullptr) slot = acquire_slot(obj);
  if (slot == nullptr) {  // pool full: exact final state, no motion
    if (shown)
      lv_obj_set_hidden(obj, false);
    else
      lv_obj_set_hidden(obj, true);
    return;
  }
  own(*slot, PROP_OPA);
  slot->generation++;  // invalidates a pending hide completion

  const PropTransition &t = resolve(opt.transition, Prop::VISIBLE);
  const Style style = shown ? t.style : t.hide_style;
  // Policy row 3 looks at ancestors only: the target's own HIDDEN flag is exactly what this call changes.
  const bool animate = style != Style::NONE && resolve_duration(t.duration) != 0 && transition_allowed(obj, false, opt);
  const uint32_t duration = resolve_duration(t.duration);

  if (shown) {
    slot->flags &= static_cast<uint8_t>(~SLOT_HIDING);
    lv_obj_set_hidden(obj, false);  // visible immediately, even while fading in
    if (!animate) {
      lv_anim_delete(slot, exec_opa);
      slot->base_opa = slot->visible_opa;
      compose(*slot, PROP_OPA);
      return;
    }
    // From a hidden target the fade starts at 0; reversing a hide continues from the current opacity.
    start_opa(slot, hidden ? 0 : slot->base_opa, slot->visible_opa, t, duration, false);
    return;
  }

  if (!animate || slot->base_opa == 0) {
    lv_anim_delete(slot, exec_opa);
    finish_hide(slot);
    return;
  }
  slot->flags |= SLOT_HIDING;
  start_opa(slot, slot->base_opa, 0, t, duration, true);
}

void set_opacity(lv_obj_t *obj, lv_opa_t opa, Opt opt) {
  if (obj == nullptr) return;
  Slot *slot = find_slot(obj);
  if (slot == nullptr || !(slot->props & PROP_OPA)) {
    // Not composed yet: the style value is the base.  Same value or a hidden target need no slot.
    if (lv_obj_get_style_opa(obj, LV_PART_MAIN) == opa) return;
    if (lv_obj_is_hidden(obj)) {
      lv_obj_set_style_opa(obj, opa, LV_PART_MAIN);  // picked up as the shown opacity when a slot appears
      return;
    }
    if (slot == nullptr) slot = acquire_slot(obj);
    if (slot == nullptr) {
      lv_obj_set_style_opa(obj, opa, LV_PART_MAIN);
      return;
    }
    own(*slot, PROP_OPA);
  }

  if (slot->flags & SLOT_HIDING) {
    // Going away: only remember the opacity for the next show.  Leaving the hide untouched keeps the final
    // visibility independent of when this call lands inside the fade; finish_hide restores base_opa = visible_opa.
    slot->visible_opa = opa;
    return;
  }
  if (lv_obj_is_hidden(obj)) {  // not shown: remember it for the next show (6)
    slot->visible_opa = slot->base_opa = opa;
    compose(*slot, PROP_OPA);
    return;
  }
  const lv_anim_t *running = lv_anim_get(slot, exec_opa);
  if (running != nullptr ? running->end_value == opa : slot->base_opa == opa) return;

  slot->visible_opa = opa;
  const PropTransition &t = resolve(opt.transition, Prop::OPACITY);
  const uint32_t duration = resolve_duration(t.duration);
  if (duration == 0 || slot->base_opa == opa || !transition_allowed(obj, true, opt)) {
    lv_anim_delete(slot, exec_opa);
    slot->base_opa = opa;
    compose(*slot, PROP_OPA);
    return;
  }
  start_opa(slot, slot->base_opa, opa, t, duration, false);
}

// ---- value (bar / arc / slider / image rotation), x, y --------------------------------------------------------------

#if LV_USE_BAR
static void exec_bar(void *var, int32_t v) { lv_bar_set_value(static_cast<lv_obj_t *>(var), v, LV_ANIM_OFF); }
#endif
#if LV_USE_SLIDER
static void exec_slider(void *var, int32_t v) { lv_slider_set_value(static_cast<lv_obj_t *>(var), v, LV_ANIM_OFF); }
#endif
#if LV_USE_ARC
static void exec_arc(void *var, int32_t v) { lv_arc_set_value(static_cast<lv_obj_t *>(var), v); }
#endif
#if LV_USE_IMAGE
static void exec_image_rotation(void *var, int32_t v) { lv_image_set_rotation(static_cast<lv_obj_t *>(var), v); }
#endif
static void exec_x(void *var, int32_t v) { lv_obj_set_x(static_cast<lv_obj_t *>(var), v); }
static void exec_y(void *var, int32_t v) { lv_obj_set_y(static_cast<lv_obj_t *>(var), v); }

// Ranged widgets: move to `value` clamped into [lo, hi]; NaN goes to `lo` without motion.
static void set_ranged(lv_obj_t *obj, lv_anim_exec_xcb_t exec, int32_t current, int32_t lo, int32_t hi, float value,
                       const Opt &opt) {
  const bool finite = std::isfinite(value);
  const int32_t target =
      finite ? static_cast<int32_t>(std::lround(std::clamp(value, static_cast<float>(lo), static_cast<float>(hi)))) : lo;
  // Same as the running target, or same as the current value with nothing running: nothing to do (7.4).
  const lv_anim_t *running = lv_anim_get(obj, exec);
  if (running != nullptr ? running->end_value == target : current == target) return;
  move_slotless(obj, exec, Prop::VALUE, current, target, target, !finite, opt);
}

#if LV_USE_IMAGE
static int32_t wrap_tenth_degrees(int32_t v) { return ((v % 3600) + 3600) % 3600; }
#endif

void set_value(lv_obj_t *obj, float value, Opt opt) {
  if (obj == nullptr) return;
  // lv_obj_check_type is an exact class match, so the order of these tests does not matter.
#if LV_USE_SLIDER
  if (lv_obj_check_type(obj, &lv_slider_class)) {
    set_ranged(obj, exec_slider, lv_slider_get_value(obj), lv_slider_get_min_value(obj), lv_slider_get_max_value(obj),
               value, opt);
    return;
  }
#endif
#if LV_USE_BAR
  if (lv_obj_check_type(obj, &lv_bar_class)) {
    set_ranged(obj, exec_bar, lv_bar_get_value(obj), lv_bar_get_min_value(obj), lv_bar_get_max_value(obj), value, opt);
    return;
  }
#endif
#if LV_USE_ARC
  if (lv_obj_check_type(obj, &lv_arc_class)) {
    set_ranged(obj, exec_arc, lv_arc_get_value(obj), lv_arc_get_min_value(obj), lv_arc_get_max_value(obj), value, opt);
    return;
  }
#endif
#if LV_USE_IMAGE
  if (lv_obj_check_type(obj, &lv_image_class)) {
    const int32_t current = wrap_tenth_degrees(lv_image_get_rotation(obj));
    const bool finite = std::isfinite(value);
    const int32_t target = finite ? wrap_tenth_degrees(static_cast<int32_t>(std::lround(std::fmod(value, 360.0f) * 10.0f))) : 0;
    const lv_anim_t *running = lv_anim_get(obj, exec_image_rotation);
    if (running != nullptr ? wrap_tenth_degrees(running->end_value) == target : current == target) return;
    // LVGL keeps image rotation in [0, 3600): interpolate along the shortest way round.
    const int32_t delta = ((target - current + 5400) % 3600 + 3600) % 3600 - 1800;
    move_slotless(obj, exec_image_rotation, Prop::VALUE, current, current + delta, target, !finite, opt);
    return;
  }
#endif
  warn_once(0, "set_value: only bar, arc, slider and image targets are supported (label counting arrives later)");
}

void set_x(lv_obj_t *obj, int32_t x, Opt opt) {
  if (obj == nullptr) return;
  const int32_t current = lv_obj_get_style_x(obj, LV_PART_MAIN);
  const lv_anim_t *running = lv_anim_get(obj, exec_x);
  if (running != nullptr ? running->end_value == x : current == x) return;
  // Percent coordinates are encoded values, not pixels: they cannot be interpolated.
  move_slotless(obj, exec_x, Prop::POSITION, current, x, x, LV_COORD_IS_PCT(current) || LV_COORD_IS_PCT(x), opt);
}

void set_y(lv_obj_t *obj, int32_t y, Opt opt) {
  if (obj == nullptr) return;
  const int32_t current = lv_obj_get_style_y(obj, LV_PART_MAIN);
  const lv_anim_t *running = lv_anim_get(obj, exec_y);
  if (running != nullptr ? running->end_value == y : current == y) return;
  move_slotless(obj, exec_y, Prop::POSITION, current, y, y, LV_COORD_IS_PCT(current) || LV_COORD_IS_PCT(y), opt);
}

// ---- text --------------------------------------------------------------------------------------------------------

// The text fade is a transition-style contributor (an opacity offset that ends at 100%), so it composes with a
// running show/hide fade instead of fighting it.
static void exec_fade(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  slot->style.progress = static_cast<uint16_t>(std::clamp<int32_t>(v, 0, LV_OPA_COVER));
  compose(*slot, PROP_OPA);
}

static void clear_fade(Slot *slot) {
  slot->style = Instance{};
  compose(*slot, PROP_OPA);
}

static void fade_done(lv_anim_t *anim) { clear_fade(static_cast<Slot *>(anim->var)); }

void set_text(lv_obj_t *obj, const char *text, Opt opt) {
  if (obj == nullptr) return;
  if (text == nullptr) text = "";
#if LV_USE_LABEL
  if (!lv_obj_check_type(obj, &lv_label_class)) {
    warn_once(1, "set_text: target is not a label");
    return;
  }
  const char *current = lv_label_get_text(obj);
  if (current != nullptr && std::strcmp(current, text) == 0) return;
  lv_label_set_text(obj, text);  // the state is exact first; the fade is decoration

  Slot *slot = find_slot(obj);
  const PropTransition &t = resolve(opt.transition, Prop::TEXT);
  const uint32_t duration = t.style == Style::FADE ? resolve_duration(t.duration) : 0;
  const uint8_t from = static_cast<uint8_t>(std::min<uint32_t>(t.from, 100) * LV_OPA_COVER / 100);
  if (duration == 0 || from >= LV_OPA_COVER || !transition_allowed(obj, true, opt)) {
    if (slot != nullptr && slot->style.kind == InstanceKind::FADE_IN) {
      lv_anim_delete(slot, exec_fade);
      clear_fade(slot);
    }
    return;
  }
  if (slot == nullptr) slot = acquire_slot(obj);
  if (slot == nullptr) return;  // pool full: plain text swap
  own(*slot, PROP_OPA);
  slot->style = Instance{&t, from, InstanceKind::FADE_IN, 0};
  start_anim(slot, exec_fade, from, LV_OPA_COVER, duration, t.delay, t.curve, fade_done);
#else
  (void) opt;
  warn_once(1, "set_text: LVGL label support is disabled");
#endif
}

// ---- color -------------------------------------------------------------------------------------------------------

static void exec_color(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  if (v <= 0)
    slot->base_color = slot->color_from;
  else if (v >= LV_OPA_COVER)
    slot->base_color = slot->color_to;  // exact end color, no mixing rounding
  else
    slot->base_color = lv_color_mix(slot->color_to, slot->color_from, static_cast<uint8_t>(v));
  compose(*slot, PROP_COLOR);
}

void set_color(lv_obj_t *obj, lv_color_t color, Opt opt) {
  if (obj == nullptr) return;
  Slot *slot = find_slot(obj);
  if (slot == nullptr || !(slot->props & PROP_COLOR)) {
    const ColorTarget target = color_target_of(obj);
    // An image with recolor opacity 0 shows no tint at all, so "same color" is not yet the requested look.
    const bool untinted = target == ColorTarget::IMAGE && lv_obj_get_style_image_recolor_opa(obj, LV_PART_MAIN) == 0;
    if (!untinted && lv_color_eq(read_color(obj, target), color)) return;
    if (slot == nullptr) slot = acquire_slot(obj);
    if (slot == nullptr) {
      write_color(obj, target, color);
      return;
    }
    own(*slot, PROP_COLOR);
    if (untinted) {
      // The LVGL default (black) is not a color anyone asked for: turn the tint on directly at the target color
      // instead of animating from a black silhouette.
      slot->base_color = slot->color_from = slot->color_to = color;
      write_color(obj, target, color);
    }
  }
  // Same target: already there, or already moving there (base and target only differ while a transition runs).
  if (lv_color_eq(slot->color_to, color)) return;

  slot->color_from = slot->base_color;
  slot->color_to = color;
  const PropTransition &t = resolve(opt.transition, Prop::COLOR);
  const uint32_t duration = resolve_duration(t.duration);
  if (duration == 0 || !transition_allowed(obj, true, opt)) {
    lv_anim_delete(slot, exec_color);
    slot->base_color = color;
    compose(*slot, PROP_COLOR);
    return;
  }
  start_anim(slot, exec_color, 0, LV_OPA_COVER, duration, t.delay, t.curve);
}

// ---- rotation (transform) ----------------------------------------------------------------------------------------

static void exec_rotation(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  slot->base_rotation = static_cast<int16_t>(std::clamp<int32_t>(v, INT16_MIN, INT16_MAX));
  compose(*slot, PROP_ROTATION);
}

void set_rotation(lv_obj_t *obj, float degrees, Opt opt) {
  if (obj == nullptr) return;
  const bool finite = std::isfinite(degrees);
  const int32_t target =
      finite ? static_cast<int32_t>(std::lround(std::clamp(degrees, -3200.0f, 3200.0f) * 10.0f)) : 0;
  Slot *slot = find_slot(obj);
  if (slot == nullptr || !(slot->props & PROP_ROTATION)) {
    if (lv_obj_get_style_transform_rotation(obj, LV_PART_MAIN) == target) return;
    if (slot == nullptr) slot = acquire_slot(obj);
    if (slot == nullptr) {
      lv_obj_set_style_transform_rotation(obj, target, LV_PART_MAIN);
      return;
    }
    own(*slot, PROP_ROTATION);
  }
  const lv_anim_t *running = lv_anim_get(slot, exec_rotation);
  if (running != nullptr ? running->end_value == target : slot->base_rotation == target) return;

  const PropTransition &t = resolve(opt.transition, Prop::ROTATION);
  const uint32_t duration = resolve_duration(t.duration);
  if (!finite || duration == 0 || slot->base_rotation == target || !transition_allowed(obj, true, opt)) {
    lv_anim_delete(slot, exec_rotation);
    exec_rotation(slot, target);
    return;
  }
  start_anim(slot, exec_rotation, slot->base_rotation, target, duration, t.delay, t.curve);
}

// ---- stop / start / policy switches --------------------------------------------------------------------------------

void stop(lv_obj_t *obj) {
  if (obj == nullptr) return;
  // Slot-less motion.  Types that do not match have nothing running, so no type test is needed.
  snap(obj, exec_x);
  snap(obj, exec_y);
#if LV_USE_BAR
  snap(obj, exec_bar);
#endif
#if LV_USE_SLIDER
  snap(obj, exec_slider);
#endif
#if LV_USE_ARC
  snap(obj, exec_arc);
#endif
#if LV_USE_IMAGE
  snap(obj, exec_image_rotation);
#endif
  Slot *slot = find_slot(obj);
  if (slot == nullptr) return;
  snap(slot, exec_opa);  // a running hide completes: HIDDEN is set
  snap(slot, exec_color);
  snap(slot, exec_rotation);
  snap(slot, exec_fade);
  // P2: stop effect and loop instances here (offsets back to 0).
}

void start() { detail::g_core.ready = true; }
void set_enabled(bool enabled) { detail::g_core.enabled = enabled; }
void set_reduced_motion(bool reduced) { detail::g_core.reduced_motion = reduced; }

// ---- user channel ------------------------------------------------------------------------------------------------

// An instantly applied tween runs on_done synchronously inside tween(), so on_done may re-enter fx calls.
static void tween_done(lv_anim_t *anim) {
  reinterpret_cast<void (*)(void *)>(anim->user_data)(anim->var);
}

void tween(void *var, lv_anim_exec_xcb_t setter, int32_t from, int32_t to, TweenOpt opt) {
  if (var == nullptr || setter == nullptr) return;
  Opt policy;
  policy.force = opt.force;
  if (from == to || !transition_allowed(opt.target, true, policy)) {
    lv_anim_delete(var, setter);
    setter(var, to);
    if (opt.on_done != nullptr) opt.on_done(var);
    return;
  }
  const uint32_t duration = opt.duration != 0 ? opt.duration : detail::g_core.motion.normal;
  start_anim(var, setter, from, to, duration, opt.delay, opt.curve, opt.on_done != nullptr ? tween_done : nullptr,
             reinterpret_cast<void *>(opt.on_done));
}

void cancel_tween(void *var, lv_anim_exec_xcb_t setter) {
  if (var != nullptr && setter != nullptr) lv_anim_delete(var, setter);
}

// ---- component ---------------------------------------------------------------------------------------------------

void LvglFx::set_motion(uint16_t fast, uint16_t normal, uint16_t slow, float intensity) {
  Motion &m = detail::g_core.motion;
  m.fast = fast;
  m.normal = normal;
  m.slow = slow;
  m.intensity = intensity;
}

void LvglFx::setup() {
  detail::ensure_pool(detail::g_core);
  // The per-iteration epoch only matters for the suppress_when cache.
  if (detail::g_core.suppress_fn == nullptr) this->disable_loop();
}

void LvglFx::loop() { detail::g_core.loop_epoch++; }

void LvglFx::dump_config() {
  const detail::Core &c = detail::g_core;
  ESP_LOGCONFIG(TAG,
                "LVGL FX:\n"
                "  Motion: fast=%ums normal=%ums slow=%ums intensity=%.2f\n"
                "  Max targets: %u (slot %u bytes, %s)\n"
                "  suppress_when: %s",
                c.motion.fast, c.motion.normal, c.motion.slow, c.motion.intensity, c.capacity,
                static_cast<unsigned>(sizeof(Slot)), c.slots_in_psram ? "PSRAM preferred" : "internal RAM",
                c.suppress_fn != nullptr ? "set" : "none");
}

}  // namespace lvgl_fx
}  // namespace esphome
