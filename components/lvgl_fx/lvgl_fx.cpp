#include "lvgl_fx.h"

#include <algorithm>
#include <new>
#include <cmath>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lvgl_fx {

static const char *const TAG = "lvgl_fx";

#if INTPTR_MAX == INT32_MAX
// Per-target slot budget (measured: 68 bytes on the 32-bit targets); keep it from creeping up
// unnoticed.
static_assert(sizeof(Slot) <= 68, "lvgl_fx::Slot grew past its size budget");
#endif

// ---- state -------------------------------------------------------------------------------------------------------

namespace detail {

// Constant-initialized: no guard variable on access.
static Core g_core;
Core &core() { return g_core; }

// Allocated once, never per frame.  PSRAM first when asked for, with the allocator's internal fallback.
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

// Frees the slot with its target.  lv_obj_delete only removes animations whose var is the object itself; ours (opacity,
// color, rotation, text fade and both effect instances) use the slot as var, so they must go here before the slot can
// be reused.
static void on_target_deleted(lv_event_t *e) {
  Core &c = g_core;
  auto *slot = static_cast<Slot *>(lv_event_get_user_data(e));
  // A child's DELETE can bubble up to a handler registered on its parent: only react to our own target.
  if (slot->obj == nullptr || lv_event_get_target(e) != slot->obj) return;
  lv_anim_delete(slot, nullptr);
  release_effects(*slot);
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

bool target_hidden(lv_obj_t *obj, bool check_self) {
  for (lv_obj_t *o = check_self ? obj : lv_obj_get_parent(obj); o != nullptr; o = lv_obj_get_parent(o)) {
    if (lv_obj_is_hidden(o)) return true;
  }
  return false;
}

// `suppress_when` runs at most once per main-loop iteration (LvglFx::loop() bumps the epoch).
bool suppressed() {
  Core &c = g_core;
  if (c.suppress_fn == nullptr) return false;
  if (c.suppress_epoch != c.loop_epoch) {
    c.suppress_epoch = c.loop_epoch;
    c.suppress_cached = c.suppress_fn();
  }
  return c.suppress_cached;
}

// Rows 1-4 of the policy table, cheapest first so the lambda only runs when everything else says "animate".
bool transition_allowed(lv_obj_t *obj, bool check_self_hidden, const Opt &opt) {
  const Core &c = g_core;
  if (opt.force) return true;
  if (!c.enabled || c.reduced_motion) return false;
  if (!c.ready) return false;
  if (obj != nullptr && target_hidden(obj, check_self_hidden)) return false;
  return opt.always || (opt.transition != nullptr && opt.transition->policy == EffectPolicy::ALWAYS) || !suppressed();
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

// First level of the chain whose field is set.
template <typename T>
static T first_set(const PropTransition *const (&levels)[3], T PropTransition::*field, T unset) {
  for (const PropTransition *level : levels) {
    if (level != nullptr && level->*field != unset) return level->*field;
  }
  return unset;
}

ResolvedTransition resolve(TransitionRef transition, Prop prop) {
  const size_t i = static_cast<size_t>(prop);
  const TransitionDef *defaults = g_core.defaults;
  const PropTransition *const levels[3] = {transition != nullptr ? transition->props[i] : nullptr,
                                            defaults != nullptr ? defaults->props[i] : nullptr,
                                            BUILTIN_TRANSITIONS.props[i]};
  ResolvedTransition r;
  r.duration = resolve_duration(first_set(levels, &PropTransition::duration, DUR_DEFAULT));
  const uint16_t delay = first_set(levels, &PropTransition::delay, UNSET_U16);
  r.delay = delay == UNSET_U16 ? 0 : delay;
  const Curve curve = first_set(levels, &PropTransition::curve, Curve::DEFAULT);
  r.curve = curve == Curve::DEFAULT ? Curve::LINEAR : curve;
  // `style` and `hide_style`: an entry that names a style but no hide style means "both" and so shields the lower
  // levels' hide style (4.2.1: `style: fade` vs `show: slide_up, hide: fade`).
  r.style = r.hide_style = Style::DEFAULT;
  for (const PropTransition *level : levels) {
    if (level == nullptr) continue;
    if (r.hide_style == Style::DEFAULT && level->hide_style != Style::DEFAULT) r.hide_style = level->hide_style;
    if (r.style == Style::DEFAULT && level->style != Style::DEFAULT) {
      r.style = level->style;
      if (r.hide_style == Style::DEFAULT && level->hide_style == Style::DEFAULT) r.hide_style = level->style;
    }
  }
  if (r.style == Style::DEFAULT) r.style = Style::NONE;
  if (r.hide_style == Style::DEFAULT) r.hide_style = r.style;
  const uint8_t from = first_set(levels, &PropTransition::from, UNSET_U8);
  r.from = from == UNSET_U8 ? 100 : from;
  const uint16_t distance = first_set(levels, &PropTransition::distance, UNSET_U16);
  r.distance = distance == UNSET_U16 ? 0 : distance;
  const char *format = first_set<const char *>(levels, &PropTransition::format, nullptr);
  r.format = format != nullptr ? format : "%.0f";
  return r;
}

}  // namespace detail

using detail::acquire_slot;
using detail::find_slot;
using detail::resolve;
using detail::resolve_duration;
using detail::transition_allowed;

Motion &motion() { return detail::g_core.motion; }
EffectOptions &effect_defaults() { return detail::g_core.effect_defaults; }

void set_preset(EffectRef builtin, const Track *adjust) {
  if (builtin == nullptr || builtin->builtin == 0 || builtin->builtin > BUILTIN_COUNT) return;
  detail::g_core.presets[builtin->builtin - 1] = adjust;
}

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
    case Curve::DEFAULT:
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
    case Curve::DEFAULT:
      break;
  }
  return t;
}

// ---- animation plumbing ------------------------------------------------------------------------------------------

// Key rules for lv_anim: (var, exec_cb) identifies an animation.  lv_anim_start replaces a running animation with the
// same pair, so retargeting needs no explicit delete.  Slot-based motion uses var = slot, one exec_cb per property;
// slot-less motion (value, x, y) uses var = the widget.  Both are unique per property; neither can collide with a
// user tween, whose exec_cb is the user's.
namespace detail {

bool start_anim(void *var, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t duration, uint32_t delay,
                Curve curve, lv_anim_completed_cb_t done, void *user_data, bool early_apply, lv_anim_deleted_cb_t deleted) {
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
  if (deleted != nullptr) lv_anim_set_deleted_cb(&anim, deleted);
  lv_anim_set_early_apply(&anim, early_apply);
  return lv_anim_start(&anim) != nullptr;
}

// `then`: when a transition finishes, its effect plays on the same target.  The anim's user_data carries the
// TransitionDef (nullptr = nothing to play).
void *then_of(const Opt &opt) {
  return (opt.transition != nullptr && opt.transition->then != nullptr) ? const_cast<TransitionDef *>(opt.transition)
                                                                        : nullptr;
}

static void run_then(lv_obj_t *obj, const void *transition) {
  if (transition != nullptr) play_then(obj, static_cast<TransitionRef>(transition)->then);
}

// A transition that is applied instantly (duration 0, policy, NaN, ...) still completes: its `then` plays at once.
// The policy of the effect itself applies as usual.  "Already there" early returns stay silent.
void instant_then(lv_obj_t *obj, const Opt &opt) { if (!opt.instant) run_then(obj, then_of(opt)); }

void slot_done(lv_anim_t *anim) { run_then(static_cast<Slot *>(anim->var)->obj, anim->user_data); }

// Stop one animation and run it to its end state.  Completion callbacks that complete *state* (hide, text style) run;
// `then` does not, since stop() must not start new motion.
void snap(void *var, lv_anim_exec_xcb_t exec) {
  lv_anim_t *running = lv_anim_get(var, exec);
  if (running == nullptr) return;
  lv_anim_t copy = *running;
  lv_anim_delete(var, exec);
  exec(var, copy.end_value);
  copy.user_data = nullptr;
  if (copy.completed_cb != nullptr) copy.completed_cb(&copy);
}

void warn_once(uint8_t id, const char *what) {
  static uint8_t warned = 0;
  if (warned & (1u << id)) return;
  warned |= static_cast<uint8_t>(1u << id);
  ESP_LOGW(TAG, "%s", what);
}

}  // namespace detail

using detail::instant_then;
using detail::run_then;
using detail::slot_done;
using detail::snap;
using detail::start_anim;
using detail::then_of;
using detail::warn_once;
using detail::ResolvedTransition;

static void slotless_done(lv_anim_t *anim) { run_then(static_cast<lv_obj_t *>(anim->var), anim->user_data); }

// Slot-less transition of an integer property of the widget itself (value, x, y): no base/offset composition is
// needed, and the widget's own getter is the base value, so external changes cannot desynchronize it.
static void move_slotless(lv_obj_t *obj, lv_anim_exec_xcb_t exec, Prop prop, int32_t from, int32_t to,
                          int32_t final_value, bool instant, const Opt &opt) {
  const ResolvedTransition t = resolve(opt.transition, prop);
  if (opt.instant || instant || t.duration == 0 || from == to || !transition_allowed(obj, true, opt)) {
    lv_anim_delete(obj, exec);
    exec(obj, final_value);
    if (from != to) instant_then(obj, opt);
    return;
  }
  void *then = then_of(opt);
  start_anim(obj, exec, from, to, t.duration, t.delay, t.curve, then != nullptr ? slotless_done : nullptr, then);
}

// The binding records live in generated static storage, separate from motion slots.
static Binding *bindings = nullptr;
static void binding_target_deleted(lv_event_t *event) {
  auto *binding = static_cast<Binding *>(lv_event_get_user_data(event));
  if (lv_event_get_target(event) == binding->obj) binding->obj = nullptr;
}
void LvglFx::add_binding(Binding *binding) {
  binding->next = bindings;
  bindings = binding;
}
static void install_binding(Binding *binding) {
  if (binding->obj != nullptr || binding->get == nullptr) return;
  auto *target = binding->get();
  if (target == nullptr) return;  // the widget has not been created yet
  binding->get = nullptr;  // never retry allocation or bind a recycled address
  if (lv_obj_add_event_cb(target, binding_target_deleted, LV_EVENT_DELETE, binding) != nullptr)
    binding->obj = target;
}
Opt select_transition(lv_obj_t *obj, Prop prop, Opt opt) {
  if (obj == nullptr || opt.transition != nullptr) return opt;
  for (auto *binding = bindings; binding != nullptr; binding = binding->next) {
    if (binding->prop != prop) continue;
    install_binding(binding);
    if (binding->obj == obj) {
      opt.transition = binding->transition;
      break;
    }
  }
  return opt;
}

// ---- opacity / visibility ----------------------------------------------------------------------------------------

// The visible style's offsets (slide / zoom) move with the base opacity, so every opacity step recomposes them too.
static void exec_opa(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  slot->base_opa = static_cast<uint8_t>(std::clamp<int32_t>(v, 0, LV_OPA_COVER));
  compose(*slot, static_cast<uint8_t>(PROP_OPA | instance_props(slot->vis)));
}

// The hide transition reached 0: now the target really goes away.
static void finish_hide(Slot *slot) {
  slot->flags &= static_cast<uint8_t>(~SLOT_HIDING);
  // Restore the shown opacity before hiding (an external un-hide then shows it as set); the next set_visible(true)
  // fades in from 0.
  slot->base_opa = slot->visible_opa;
  // Slide/zoom exit offsets go back to 0 together with HIDDEN: the target is invisible, so the jump cannot be seen.
  // Everything is settled before the flag changes, so an event handler may call set_visible(true) re-entrantly.
  const uint8_t offsets = instance_props(slot->vis);
  slot->vis = Instance{};
  compose(*slot, static_cast<uint8_t>(PROP_OPA | offsets));
  lv_obj_set_hidden(slot->obj, true);
}

// Every opacity animation is the only (slot, exec_opa) animation: lv_anim_start replaces a running one without
// completing it, and every set_visible / set_opacity call either starts a new one or deletes the old (kill_opa).  A
// completion therefore always belongs to the latest call - a hide that was reversed never arrives here (6, stale hide
// completions) - and needs no generation number.  A hide completes by hiding; any of them then plays its `then`.
static void opa_done(lv_anim_t *anim) {
  auto *slot = static_cast<Slot *>(anim->var);
  if (slot->flags & SLOT_HIDING)
    finish_hide(slot);
  else
    detail::drop_visible_style(slot);
  run_then(slot->obj, anim->user_data);
}

static void start_opa(Slot *slot, int32_t from, int32_t to, const ResolvedTransition &t, const Opt &opt) {
  if (start_anim(slot, exec_opa, from, to, t.duration, t.delay, t.curve, opa_done, then_of(opt))) return;
  // LVGL could not allocate the animation: the state still has to be exact, so complete right away.
  exec_opa(slot, to);
  if (slot->flags & SLOT_HIDING)
    finish_hide(slot);
  else
    detail::drop_visible_style(slot);
  instant_then(slot->obj, opt);
}

// Drop the running opacity animation and the offsets that go with it, without completing it (the caller sets the final
// opacity and composes it).
static void kill_opa(Slot *slot) {
  lv_anim_delete(slot, exec_opa);
  detail::drop_visible_style(slot);
}

void set_visible(lv_obj_t *obj, bool shown, Opt opt) {
  if (obj == nullptr) return;
  opt = select_transition(obj, Prop::VISIBLE, opt);
  const bool hidden = lv_obj_is_hidden(obj);
  Slot *slot = find_slot(obj);
  const bool hiding = slot != nullptr && (slot->flags & SLOT_HIDING);
  if (opt.instant && (slot == nullptr || !(slot->props & PROP_OPA))) {
    lv_obj_set_hidden(obj, !shown);
    return;
  }
  // Already there (or on the way): nothing to do, and a running transition keeps going.
  if (!opt.instant && (shown ? (!hidden && !hiding) : (hidden || hiding))) return;

  if (slot == nullptr) slot = acquire_slot(obj);
  if (slot == nullptr) {  // pool full: exact final state, no motion
    lv_obj_set_hidden(obj, !shown);
    instant_then(obj, opt);
    return;
  }
  own(*slot, PROP_OPA);

  const ResolvedTransition t = resolve(opt.transition, Prop::VISIBLE);
  const Style style = shown ? t.style : t.hide_style;
  // Policy row 3 looks at ancestors only: the target's own HIDDEN flag is exactly what this call changes.
  const bool animate = !opt.instant && style != Style::NONE && t.duration != 0 && transition_allowed(obj, false, opt);

  if (shown) {
    slot->flags &= static_cast<uint8_t>(~SLOT_HIDING);
    lv_obj_set_hidden(obj, false);  // visible immediately, even while fading in
    if (!animate) {
      kill_opa(slot);
      slot->base_opa = slot->visible_opa;
      compose(*slot, PROP_OPA);
      instant_then(obj, opt);
      return;
    }
    // A reversed hide continues from the current opacity (and offsets); from a hidden target everything starts at 0.
    detail::begin_visible_style(slot, style, t, hiding || slot->vis.kind != InstanceKind::NONE);
    if (slot->vis.kind != InstanceKind::NONE) slot->vis.state = slot->visible_opa;  // ends at offset 0 (set_opacity)
    if (hidden) slot->base_opa = 0;
    start_opa(slot, slot->base_opa, slot->visible_opa, t, opt);
    return;
  }

  if (!animate || slot->base_opa == 0) {
    kill_opa(slot);
    finish_hide(slot);
    instant_then(obj, opt);
    return;
  }
  const bool entry_running = slot->vis.kind != InstanceKind::NONE;
  detail::begin_visible_style(slot, style, t, entry_running);
  // A fresh exit offset starts at 0 from wherever the opacity is now (a hide during a fade-in does not jump).
  if (!entry_running && slot->vis.kind != InstanceKind::NONE) slot->vis.state = slot->base_opa;
  slot->flags |= SLOT_HIDING;
  start_opa(slot, slot->base_opa, 0, t, opt);
}

void set_opacity(lv_obj_t *obj, lv_opa_t opa, Opt opt) {
  if (obj == nullptr) return;
  opt = select_transition(obj, Prop::OPACITY, opt);
  Slot *slot = find_slot(obj);
  if (slot == nullptr || !(slot->props & PROP_OPA)) {
    // Not composed yet: the style value is the base.  Same value or a hidden target need no slot.
    if (opt.instant) {
      lv_obj_set_style_opa(obj, opa, LV_PART_MAIN);
      return;
    }
    if (lv_obj_get_style_opa(obj, LV_PART_MAIN) == opa) return;
    if (lv_obj_is_hidden(obj)) {
      lv_obj_set_style_opa(obj, opa, LV_PART_MAIN);  // picked up as the shown opacity when a slot appears
      instant_then(obj, opt);
      return;
    }
    if (slot == nullptr) slot = acquire_slot(obj);
    if (slot == nullptr) {
      lv_obj_set_style_opa(obj, opa, LV_PART_MAIN);
      instant_then(obj, opt);
      return;
    }
    own(*slot, PROP_OPA);
  }

  if (opt.instant) {
    const bool hiding = slot->flags & SLOT_HIDING;
    kill_opa(slot);
    slot->visible_opa = slot->base_opa = opa;
    if (hiding)
      finish_hide(slot);
    else
      compose(*slot, PROP_OPA);
    return;
  }
  if (slot->flags & SLOT_HIDING) {
    // Going away: only remember the opacity for the next show.  Leaving the hide untouched keeps the final
    // visibility independent of when this call lands inside the fade; finish_hide restores base_opa = visible_opa.
    slot->visible_opa = opa;
    return;
  }
  if (lv_obj_is_hidden(obj)) {  // not shown: remember it for the next show
    slot->visible_opa = slot->base_opa = opa;
    compose(*slot, PROP_OPA);
    instant_then(obj, opt);
    return;
  }
  const lv_anim_t *running = lv_anim_get(slot, exec_opa);
  if (running != nullptr ? running->end_value == opa : slot->base_opa == opa) return;

  slot->visible_opa = opa;
  // A set_opacity that lands inside a slide/zoom entry takes the opacity over: the entry offsets end right here.
  detail::drop_visible_style(slot);
  const ResolvedTransition t = resolve(opt.transition, Prop::OPACITY);
  if (t.duration == 0 || slot->base_opa == opa || !transition_allowed(obj, true, opt)) {
    kill_opa(slot);
    slot->base_opa = opa;
    compose(*slot, PROP_OPA);
    instant_then(obj, opt);
    return;
  }
  start_opa(slot, slot->base_opa, opa, t, opt);
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
  const float clamped = std::clamp(value, static_cast<float>(lo), static_cast<float>(hi));
  const int32_t target = finite ? static_cast<int32_t>(std::lround(clamped)) : lo;
  // Same as the running target, or same as the current value with nothing running: nothing to do.
  const lv_anim_t *running = lv_anim_get(obj, exec);
  if (!opt.instant && (running != nullptr ? running->end_value == target : current == target)) return;
  move_slotless(obj, exec, Prop::VALUE, current, target, target, !finite, opt);
}

#if LV_USE_IMAGE
static int32_t wrap_tenth_degrees(int32_t v) { return ((v % 3600) + 3600) % 3600; }
#endif

void set_value(lv_obj_t *obj, float value, Opt opt) {
  if (obj == nullptr) return;
#if LV_USE_LABEL
  if (lv_obj_check_type(obj, &lv_label_class)) {
    detail::set_label_value(obj, value, select_transition(obj, Prop::TEXT, opt));
    return;
  }
#endif
  opt = select_transition(obj, Prop::VALUE, opt);
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
    const int32_t tenths = finite ? static_cast<int32_t>(std::lround(std::fmod(value, 360.0f) * 10.0f)) : 0;
    const int32_t target = finite ? wrap_tenth_degrees(tenths) : 0;
    const lv_anim_t *running = lv_anim_get(obj, exec_image_rotation);
    if (!opt.instant && (running != nullptr ? wrap_tenth_degrees(running->end_value) == target : current == target)) return;
    // LVGL keeps image rotation in [0, 3600): interpolate along the shortest way round.
    const int32_t delta = ((target - current + 5400) % 3600 + 3600) % 3600 - 1800;
    move_slotless(obj, exec_image_rotation, Prop::VALUE, current, current + delta, target, !finite, opt);
    return;
  }
#endif
  warn_once(0, "set_value: only bar, arc, slider, image and label targets are supported");
}

void set_x(lv_obj_t *obj, int32_t x, Opt opt) {
  if (obj == nullptr) return;
  opt = select_transition(obj, Prop::POSITION, opt);
  const int32_t current = lv_obj_get_style_x(obj, LV_PART_MAIN);
  const lv_anim_t *running = lv_anim_get(obj, exec_x);
  if (!opt.instant && (running != nullptr ? running->end_value == x : current == x)) return;
  // Percent coordinates are encoded values, not pixels: they cannot be interpolated.
  move_slotless(obj, exec_x, Prop::POSITION, current, x, x, LV_COORD_IS_PCT(current) || LV_COORD_IS_PCT(x), opt);
}

void set_y(lv_obj_t *obj, int32_t y, Opt opt) {
  if (obj == nullptr) return;
  opt = select_transition(obj, Prop::POSITION, opt);
  const int32_t current = lv_obj_get_style_y(obj, LV_PART_MAIN);
  const lv_anim_t *running = lv_anim_get(obj, exec_y);
  if (!opt.instant && (running != nullptr ? running->end_value == y : current == y)) return;
  move_slotless(obj, exec_y, Prop::POSITION, current, y, y, LV_COORD_IS_PCT(current) || LV_COORD_IS_PCT(y), opt);
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

namespace detail {
bool color_transition_running(Slot &slot) { return lv_anim_get(&slot, exec_color) != nullptr; }
}  // namespace detail

void set_color(lv_obj_t *obj, lv_color_t color, Opt opt) {
  if (obj == nullptr) return;
  opt = select_transition(obj, Prop::COLOR, opt);
  Slot *slot = find_slot(obj);
  const ColorTarget target = color_target_of(obj);
  if (slot == nullptr || !(slot->props & PROP_COLOR)) {
    // An image with recolor opacity 0 shows no tint at all, so "same color" is not yet the requested look.
    const bool untinted = target == ColorTarget::IMAGE && lv_obj_get_style_image_recolor_opa(obj, LV_PART_MAIN) == 0;
    if (opt.instant) {
      write_color(obj, target, color);
      return;
    }
    if (!untinted && lv_color_eq(read_color(obj, target), color)) return;
    if (slot == nullptr) slot = acquire_slot(obj);
    if (slot == nullptr) {
      write_color(obj, target, color);
      instant_then(obj, opt);
      return;
    }
    own(*slot, PROP_COLOR);
  } else if (slot->color_target != target) {
    // The base color always belongs to set_color: a channel an effect picked (flash on bg_color of a label) gives way.
    // The old property is restored and a running flash simply continues on the new one.
    lv_anim_delete(slot, exec_color);
    own_color(*slot, target, false);
  }
  if (slot->flags & SLOT_UNTINTED) {
    // The LVGL default (black) is not a color anyone asked for: turn the tint on directly at the target color
    // instead of animating from a black silhouette.  Any flash running on top keeps composing over it.
    lv_anim_delete(slot, exec_color);
    slot->flags &= static_cast<uint8_t>(~SLOT_UNTINTED);
    slot->base_color = slot->color_from = slot->color_to = color;
    lv_obj_set_style_image_recolor_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    compose(*slot, PROP_COLOR);
    instant_then(obj, opt);
    return;
  }
  // Same target: already there, or already moving there (base and target only differ while a transition runs).
  if (!opt.instant && lv_color_eq(slot->color_to, color)) return;

  slot->color_from = slot->base_color;
  slot->color_to = color;
  const ResolvedTransition t = resolve(opt.transition, Prop::COLOR);
  if (opt.instant || t.duration == 0 || !transition_allowed(obj, true, opt)) {
    lv_anim_delete(slot, exec_color);
    slot->base_color = color;
    compose(*slot, PROP_COLOR);
    instant_then(obj, opt);
    return;
  }
  start_anim(slot, exec_color, 0, LV_OPA_COVER, t.duration, t.delay, t.curve, slot_done, then_of(opt));
}

// ---- rotation (transform) ----------------------------------------------------------------------------------------

static void exec_rotation(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  slot->base_rotation = static_cast<int16_t>(std::clamp<int32_t>(v, INT16_MIN, INT16_MAX));
  compose(*slot, PROP_ROTATION);
}

void set_rotation(lv_obj_t *obj, float degrees, Opt opt) {
  if (obj == nullptr) return;
  opt = select_transition(obj, Prop::ROTATION, opt);
  const bool finite = std::isfinite(degrees);
  const int32_t target =
      finite ? static_cast<int32_t>(std::lround(std::clamp(degrees, -3200.0f, 3200.0f) * 10.0f)) : 0;
  Slot *slot = find_slot(obj);
  if (slot == nullptr || !(slot->props & PROP_ROTATION)) {
    if (opt.instant) {
      lv_obj_set_style_transform_rotation(obj, target, LV_PART_MAIN);
      return;
    }
    if (lv_obj_get_style_transform_rotation(obj, LV_PART_MAIN) == target) return;
    if (slot == nullptr) slot = acquire_slot(obj);
    if (slot == nullptr) {
      lv_obj_set_style_transform_rotation(obj, target, LV_PART_MAIN);
      instant_then(obj, opt);
      return;
    }
    own(*slot, PROP_ROTATION);
  }
  const lv_anim_t *running = lv_anim_get(slot, exec_rotation);
  if (!opt.instant && (running != nullptr ? running->end_value == target : slot->base_rotation == target)) return;

  const ResolvedTransition t = resolve(opt.transition, Prop::ROTATION);
  if (opt.instant || !finite || t.duration == 0 || slot->base_rotation == target || !transition_allowed(obj, true, opt)) {
    lv_anim_delete(slot, exec_rotation);
    exec_rotation(slot, target);
    instant_then(obj, opt);
    return;
  }
  start_anim(slot, exec_rotation, slot->base_rotation, target, t.duration, t.delay, t.curve, slot_done,
             then_of(opt));
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
  // stop() starts no new motion: snap() never plays a `then`.
  snap(slot, exec_opa);          // a running hide completes: HIDDEN is set, slide/zoom offsets are back at 0
  snap(slot, exec_color);
  snap(slot, exec_rotation);
  detail::stop_text(slot);       // text fade/roll/count: the final text, offsets 0
  detail::drop_visible_style(slot);
  detail::stop_effects(*slot);   // one-shot and loop end at rest, a deferred effect is dropped
}

void start() { detail::g_core.ready = true; }
void set_enabled(bool enabled) { detail::g_core.enabled = enabled; }
void set_reduced_motion(bool reduced) { detail::g_core.reduced_motion = reduced; }

void set_image_opa(lv_obj_t *obj, uint8_t opacity) {
  if (obj == nullptr) return;
  Slot *slot = find_slot(obj);
  if (slot == nullptr || !(slot->props & PROP_IMAGE_OPA)) {
    lv_obj_set_style_image_opa(obj, opacity, LV_PART_MAIN);
    return;
  }
  slot->base_image_opa = opacity;
  compose(*slot, PROP_IMAGE_OPA);
}

// ---- user channel ------------------------------------------------------------------------------------------------

// Completion metadata exists only for cross-widget chains and is released on completion or deletion.
struct TweenCompletion {
  TweenCompletion *next;
  void (*on_done)(void *);
  EffectRef effect;
  lv_obj_t *target;
};
static TweenCompletion *tween_completions = nullptr;
static void completion_target_deleted(lv_event_t *event) {
  auto *completion = static_cast<TweenCompletion *>(lv_event_get_user_data(event));
  completion->target = nullptr;
  completion->effect = nullptr;
}
static void chained_tween_done(lv_anim_t *anim) {
  auto *completion = static_cast<TweenCompletion *>(anim->user_data);
  auto callback = completion->on_done;
  if (callback != nullptr) callback(anim->var);
  // LVGL unlinks the completed animation before on_done and deletes it afterwards.
  // A target deleted by on_done disarms this still-live completion via LV_EVENT_DELETE.
  if (completion->effect != nullptr && completion->target != nullptr)
    play(completion->target, completion->effect);
}
static void release_completion(TweenCompletion *completion) {
  auto **link = &tween_completions;
  while (*link != nullptr && *link != completion) link = &(*link)->next;
  if (*link != nullptr) *link = completion->next;
  if (completion->target != nullptr)
    lv_obj_remove_event_cb_with_user_data(completion->target, completion_target_deleted, completion);
  delete completion;
}
static void chained_tween_deleted(lv_anim_t *anim) {
  release_completion(static_cast<TweenCompletion *>(anim->user_data));
}
static void tween_done(lv_anim_t *anim) {
  reinterpret_cast<void (*)(void *)>(anim->user_data)(anim->var);
}
void cancel_tween_effect(lv_obj_t *target) {
  for (auto *entry = tween_completions; entry != nullptr; entry = entry->next)
    if (entry->target == target) entry->effect = nullptr;
}
void tween(void *var, lv_anim_exec_xcb_t setter, int32_t from, int32_t to, TweenOpt opt) {
  if (var == nullptr || setter == nullptr) return;
  Opt policy;
  policy.force = opt.force;
  policy.always = opt.always;
  if (from == to || !transition_allowed(opt.target, true, policy)) {
    lv_anim_delete(var, setter);
    setter(var, to);
    if (opt.on_done != nullptr) opt.on_done(var);
    return;
  }
  const uint32_t duration = opt.duration != 0 ? opt.duration : detail::g_core.motion.normal;
  if (opt.completion_effect == nullptr || opt.completion_target == nullptr) {
    start_anim(var, setter, from, to, duration, opt.delay, opt.curve, opt.on_done != nullptr ? tween_done : nullptr,
               reinterpret_cast<void *>(opt.on_done));
    return;
  }
  auto *completion = new (std::nothrow) TweenCompletion{tween_completions, opt.on_done, opt.completion_effect, opt.completion_target};
  if (completion == nullptr) {
    start_anim(var, setter, from, to, duration, opt.delay, opt.curve, opt.on_done != nullptr ? tween_done : nullptr,
               reinterpret_cast<void *>(opt.on_done));
    return;
  }
  tween_completions = completion;
  if (lv_obj_add_event_cb(completion->target, completion_target_deleted, LV_EVENT_DELETE, completion) == nullptr) {
    release_completion(completion);
    start_anim(var, setter, from, to, duration, opt.delay, opt.curve, opt.on_done != nullptr ? tween_done : nullptr,
               reinterpret_cast<void *>(opt.on_done));
    return;
  }
  if (!start_anim(var, setter, from, to, duration, opt.delay, opt.curve, chained_tween_done, completion, true,
                  chained_tween_deleted))
    release_completion(completion);
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
  for (auto *binding = bindings; binding != nullptr; binding = binding->next) install_binding(binding);
  detail::Core &c = detail::g_core;
  detail::ensure_pool(c);
  c.owner = this;
  // loop() has work when suppress_when needs its per-iteration epoch, or while there are deferred effects or
  // loops to watch; fx::play() wakes it up otherwise.
  if (c.suppress_fn == nullptr && c.watching == 0) this->disable_loop();
}

void LvglFx::loop() {
  detail::Core &c = detail::g_core;
  c.loop_epoch++;
  if (c.watching == 0) {
    if (c.suppress_fn == nullptr) this->disable_loop();
    return;
  }
  detail::service_effects();
}

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
