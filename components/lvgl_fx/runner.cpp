#include "lvgl_fx.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome {
namespace lvgl_fx {

static const char *const TAG = "lvgl_fx";

// Effect runner.
//
// One lv_anim per running instance: it counts ms from 0 to the effect length and every track cuts its own
// delay/duration/curve slice out of that time (effect_contribute).  A target has one one-shot and one loop.  The
// anims use the slot as var with one exec callback per instance: (var, exec) is unique, and everything that uses the
// slot as var is deleted with the target.  Offsets are never stored: ending an instance only drops its contribution,
// and compose() writes the base value back.

namespace {

using detail::core;

// ---- options -------------------------------------------------------------------------------------------------------

// An effect's options with unset fields resolved against the global defaults.
struct Options {
  bool ignore_retrigger;
  bool defer;
  bool always;
  uint16_t repeat;
  uint16_t repeat_delay;
  uint16_t cooldown;
  uint16_t defer_timeout;
};

Options resolve_options(const EffectDef &d) {
  const EffectOptions &g = core().effect_defaults;
  Options o;
  o.ignore_retrigger = (d.retrigger != Retrigger::DEFAULT ? d.retrigger : g.retrigger) == Retrigger::IGNORE;
  o.defer = (d.when_suppressed != WhenSuppressed::DEFAULT ? d.when_suppressed : g.when_suppressed) ==
            WhenSuppressed::DEFER;
  o.always = (d.policy != EffectPolicy::DEFAULT ? d.policy : g.policy) == EffectPolicy::ALWAYS;
  o.repeat = d.repeat != 0 ? d.repeat : (g.repeat != 0 ? g.repeat : 1);
  o.repeat_delay = d.repeat_delay != UNSET_U16 ? d.repeat_delay : g.repeat_delay;
  o.cooldown = d.cooldown != UNSET_U16 ? d.cooldown : g.cooldown;
  o.defer_timeout = d.defer_timeout != UNSET_U16 ? d.defer_timeout : g.defer_timeout;
  return o;
}

// ---- bookkeeping ---------------------------------------------------------------------------------------------------

// Keeps Core::watching in step with slot_watched(): construct before changing deferred / loop-on, the destructor
// compares.  Wakes LvglFx::loop() when the first thing to watch appears.
class WatchGuard {
 public:
  explicit WatchGuard(Slot &slot) : slot_(slot), before_(detail::slot_watched(slot)) {}
  ~WatchGuard() {
    const bool after = detail::slot_watched(slot_);
    if (after == before_) return;
    detail::Core &c = core();
    if (!after) {
      c.watching--;
    } else {
      c.watching++;
      if (c.owner != nullptr) c.owner->enable_loop();
    }
  }
  WatchGuard(const WatchGuard &) = delete;
  WatchGuard &operator=(const WatchGuard &) = delete;

 private:
  Slot &slot_;
  const bool before_;
};

// Motion intensity is sampled when an effect starts (4.1: a runtime change applies to the next play), so the frame
// math stays integer.
void sample_intensity() {
  detail::Core &c = core();
  const float intensity = c.motion.intensity;
  c.intensity_q8 = !(intensity > 0.0f) ? 0 : static_cast<uint32_t>(std::min(intensity, 16.0f) * LV_SCALE_NONE + 0.5f);
}

void warn_once(bool &warned, const char *what) {
  if (warned) return;
  warned = true;
  ESP_LOGW(TAG, "%s", what);
}

// ---- instances -----------------------------------------------------------------------------------------------------

void exec_one_shot(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  slot->one_shot.progress = static_cast<uint16_t>(std::clamp<int32_t>(v, 0, UINT16_MAX));
  compose(*slot, slot->one_shot.state);
}

void exec_loop(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  slot->loop.progress = static_cast<uint16_t>(std::clamp<int32_t>(v, 0, UINT16_MAX));
  compose(*slot, slot->loop.state);
}

// The effect's tracks take ownership of the properties they drive.  A target has one color channel: a flash moves
// it to its own style property unless something else drives it (a color transition, or the other effect instance,
// `other_state`), in which case it uses the busy channel.
void own_effect(Slot &slot, const EffectDef &def, uint8_t mask, uint8_t other_state) {
  own(slot, static_cast<uint8_t>(mask & ~PROP_COLOR));
  if (mask & PROP_OUTLINE) {
    const uint8_t peak = effect_glow_peak(def);
    static bool warned = false;
    if (slot.base_outline_opa >= peak && peak != 0)
      warn_once(warned, "glow has no visible effect: the target's outline_opa is not below the glow peak; "
                        "set outline_opa: 0");
  }
  if (!(mask & PROP_COLOR)) return;
  ColorTarget target;
  switch (effect_flash_property(def)) {
    case FlashProp::BG_COLOR:
      target = ColorTarget::BG;
      break;
    case FlashProp::TEXT_COLOR:
      target = ColorTarget::TEXT;
      break;
    case FlashProp::IMAGE_RECOLOR:
      target = ColorTarget::IMAGE;
      break;
    case FlashProp::AUTO:
    default:
      target = color_target_of(slot.obj);
      break;
  }
  const bool busy = detail::color_transition_running(slot) || (other_state & PROP_COLOR) != 0;
  if (!own_color(slot, target, busy)) {
    static bool warned = false;
    warn_once(warned, "flash: the target's color is already driven through another property; using that one");
  }
}

// One lv_anim counting 0..length ms.  False when LVGL could not allocate it.
bool start_instance_anim(Slot &slot, lv_anim_exec_xcb_t exec, uint32_t length, uint16_t repeat, uint16_t repeat_delay,
                         lv_anim_completed_cb_t done) {
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, &slot);
  lv_anim_set_exec_cb(&anim, exec);
  lv_anim_set_values(&anim, 0, static_cast<int32_t>(length));
  lv_anim_set_duration(&anim, length);
  lv_anim_set_path_cb(&anim, lv_anim_path_linear);
  lv_anim_set_repeat_count(&anim, repeat == REPEAT_INFINITE ? LV_ANIM_REPEAT_INFINITE : repeat);
  lv_anim_set_repeat_delay(&anim, repeat_delay);
  if (done != nullptr) lv_anim_set_completed_cb(&anim, done);
  return lv_anim_start(&anim) != nullptr;
}

// ---- one-shot ------------------------------------------------------------------------------------------------------

// The instance stops contributing and its offsets drop to zero.  The effect stays in `def` for cooldown (5, row 5).
void end_one_shot(Slot &slot) {
  const uint8_t mask = slot.one_shot.state;
  slot.one_shot.kind = InstanceKind::NONE;
  slot.one_shot.progress = 0;
  slot.one_shot.state = 0;
  compose(slot, mask);
}

void stop_one_shot(Slot &slot) {
  lv_anim_delete(&slot, exec_one_shot);
  end_one_shot(slot);
}

void one_shot_done(lv_anim_t *anim) {
  auto *slot = static_cast<Slot *>(anim->var);
  const auto *def = static_cast<const EffectDef *>(slot->one_shot.def);
  end_one_shot(*slot);
  if (def != nullptr) detail::play_then(slot->obj, def->then);
}

// Replaces whatever one-shot is running: the old offsets drop, the new one starts at time 0.
void begin_one_shot(Slot &slot, const EffectDef &def, const Options &o, uint32_t length) {
  sample_intensity();
  const uint8_t previous = slot.one_shot.state;
  lv_anim_delete(&slot, exec_one_shot);
  // Forget the replaced one-shot's instance first so its color bit does not make the channel look busy.
  slot.one_shot.kind = InstanceKind::NONE;
  slot.one_shot.progress = 0;
  slot.one_shot.state = 0;
  const uint8_t mask = effect_props(def);
  own_effect(slot, def, mask, slot.loop.state);
  slot.one_shot = Instance{&def, 0, InstanceKind::EFFECT, mask};
  slot.last_play_tick = lv_tick_get();
  if (!start_instance_anim(slot, exec_one_shot, length, o.repeat, o.repeat_delay, one_shot_done)) end_one_shot(slot);
  compose(slot, previous);  // properties only the replaced one-shot drove
}

// ---- loop ----------------------------------------------------------------------------------------------------------

// The loop stays on but rests: offsets zero, the animation gone.  It resumes from phase 0.
void rest_loop(Slot &slot) {
  lv_anim_delete(&slot, exec_loop);
  const uint8_t mask = slot.loop.state;
  slot.loop.kind = InstanceKind::NONE;
  slot.loop.progress = 0;
  slot.loop.state = 0;
  slot.flags |= SLOT_LOOP_PAUSED;
  compose(slot, mask);
}

// Switch the loop off: at rest, and forgotten.  Callers hold a WatchGuard.
void clear_loop(Slot &slot) {
  rest_loop(slot);
  slot.loop = Instance{};
  slot.flags &= static_cast<uint8_t>(~(SLOT_LOOP_ON | SLOT_LOOP_PAUSED | SLOT_LOOP_FORCE));
}

void begin_loop(Slot &slot, const EffectDef &def, const Options &o, uint32_t length) {
  sample_intensity();
  slot.loop.kind = InstanceKind::NONE;  // a restarted loop: its old color bit must not look busy
  slot.loop.progress = 0;
  slot.loop.state = 0;
  const uint8_t mask = effect_props(def);
  own_effect(slot, def, mask, slot.one_shot.state);
  slot.loop = Instance{&def, 0, InstanceKind::EFFECT, mask};
  slot.flags &= static_cast<uint8_t>(~SLOT_LOOP_PAUSED);
  if (!start_instance_anim(slot, exec_loop, length, REPEAT_INFINITE, o.repeat_delay, nullptr)) rest_loop(slot);
}

// ---- policy --------------------------------------------------------------------------------------------------------

enum class Verdict : uint8_t { OK, DROP, SUPPRESSED };

// Rows 1-4 of the policy table, cheapest first.  Rows 1-3 never defer; row 4 only when the effect is not `always`.
Verdict judge(lv_obj_t *obj, bool always) {
  const detail::Core &c = core();
  if (!c.enabled || c.reduced_motion || !c.ready) return Verdict::DROP;
  if (detail::target_hidden(obj, true)) return Verdict::DROP;
  if (!always && detail::suppressed()) return Verdict::SUPPRESSED;
  return Verdict::OK;
}

void clear_deferred(Slot &slot) {
  WatchGuard watch(slot);
  slot.deferred = nullptr;
}

// 4.4: one deferred effect per target, a later one replaces it.
void defer_effect(lv_obj_t *obj, const EffectDef &def) {
  Slot *slot = detail::acquire_slot(obj);
  if (slot == nullptr) return;
  WatchGuard watch(*slot);
  slot->deferred = &def;
  slot->deferred_tick = lv_tick_get();
}

void play_one_shot(lv_obj_t *obj, const EffectDef &def, const Options &o, uint32_t length, bool force) {
  Slot *slot = detail::find_slot(obj);
  // The same effect is already running: `retrigger: ignore`.
  if (o.ignore_retrigger && slot != nullptr && slot->one_shot.kind == InstanceKind::EFFECT &&
      slot->one_shot.def == &def)
    return;
  if (!force) {
    switch (judge(obj, o.always)) {
      case Verdict::OK:
        break;
      case Verdict::SUPPRESSED:
        if (o.defer) defer_effect(obj, def);
        return;
      case Verdict::DROP:
        return;
    }
    // Row 5: the cooldown counts from the last time this effect started on this target.
    if (o.cooldown != 0 && slot != nullptr && slot->one_shot.def == &def &&
        lv_tick_elaps(slot->last_play_tick) < o.cooldown)
      return;
  }
  if (slot == nullptr) slot = detail::acquire_slot(obj);
  if (slot == nullptr) return;  // pool full: the effect is decoration, the state is unaffected
  begin_one_shot(*slot, def, o, length);
}

void play_loop(lv_obj_t *obj, const EffectDef &def, const Options &o, uint32_t length, bool force) {
  Slot *slot = detail::find_slot(obj);
  if (slot != nullptr && (slot->flags & SLOT_LOOP_ON) && slot->loop.def == &def) {
    // Already on: playing it again does nothing - except that `force` starts a resting loop.
    if (force) slot->flags |= SLOT_LOOP_FORCE;  // from now on policy cannot pause it
    if (!force || !(slot->flags & SLOT_LOOP_PAUSED)) return;
  } else if (slot == nullptr) {
    slot = detail::acquire_slot(obj);
    if (slot == nullptr) return;
  }
  WatchGuard watch(*slot);
  if ((slot->flags & SLOT_LOOP_ON) && slot->loop.def != &def) clear_loop(*slot);  // a different loop replaces it
  slot->loop.def = &def;
  slot->flags |= SLOT_LOOP_ON;
  if (force)
    slot->flags |= SLOT_LOOP_FORCE;
  else
    slot->flags &= static_cast<uint8_t>(~SLOT_LOOP_FORCE);
  // Not allowed to move right now (rows 1-4): remembered as on, started when it is allowed (see service_loop).
  if (force || judge(obj, o.always) == Verdict::OK)
    begin_loop(*slot, def, o, length);
  else
    slot->flags |= SLOT_LOOP_PAUSED;
}

// ---- loop() work -----------------------------------------------------------------------------------------------

void service_deferred(Slot &slot) {
  const EffectDef *def = slot.deferred;
  if (lv_tick_elaps(slot.deferred_tick) > resolve_options(*def).defer_timeout) {
    clear_deferred(slot);  // too late to be useful
    return;
  }
  if (detail::suppressed()) return;
  clear_deferred(slot);
  play(slot.obj, def);  // the other policy rows apply again
}

void service_loop(Slot &slot) {
  const auto &def = *static_cast<const EffectDef *>(slot.loop.def);
  const Options o = resolve_options(def);
  const bool allowed = (slot.flags & SLOT_LOOP_FORCE) != 0 || judge(slot.obj, o.always) == Verdict::OK;
  const bool resting = (slot.flags & SLOT_LOOP_PAUSED) != 0;
  const uint32_t length = effect_length(def);  // a preset can change it; a zero-length loop must not start
  if (resting && allowed && length != 0) {
    begin_loop(slot, def, o, length);
  } else if (!resting) {
    if (!allowed || length == 0) {
      rest_loop(slot);
    } else if (const lv_anim_t *running = lv_anim_get(&slot, exec_loop);
               running != nullptr && static_cast<uint32_t>(running->duration) != length) {
      begin_loop(slot, def, o, length);  // a motion token or preset changed the length: restart from phase 0
    }
  }
}

}  // namespace

// ---- internals shared with lvgl_fx.cpp ------------------------------------------------------------------------------

namespace detail {

void stop_effects(Slot &slot) {
  WatchGuard watch(slot);
  if (slot.one_shot.kind == InstanceKind::EFFECT) stop_one_shot(slot);
  if (slot.flags & SLOT_LOOP_ON) clear_loop(slot);
  slot.deferred = nullptr;
}

void release_effects(Slot &slot) {
  // The animations are gone already (deleted with their var, the slot); only the loop() bookkeeping is left.
  if (slot_watched(slot)) core().watching--;
}

void play_then(lv_obj_t *obj, EffectRef then) {
  if (then == nullptr) return;
  if (resolve_options(*then).repeat == REPEAT_INFINITE) {  // a loop never ends, so it is no follow-up
    static bool warned = false;
    warn_once(warned, "`then` must not be a loop effect: ignored");
    return;
  }
  play(obj, then);
}

void service_effects() {
  Core &c = core();
  for (uint8_t i = 0; i < c.used; i++) {
    Slot &slot = c.slots[i];
    if (slot.obj == nullptr) continue;
    if (slot.deferred != nullptr) service_deferred(slot);
    if (slot.flags & SLOT_LOOP_ON) service_loop(slot);
  }
}

}  // namespace detail

// ---- API -----------------------------------------------------------------------------------------------------------

void play(lv_obj_t *obj, EffectRef effect, Opt opt) {
  if (obj == nullptr || effect == nullptr) return;
  const uint32_t length = effect_length(*effect);
  if (length == 0) return;  // no tracks, or nothing with a duration
  const Options o = resolve_options(*effect);
  if (o.repeat == REPEAT_INFINITE)
    play_loop(obj, *effect, o, length, opt.force);
  else
    play_one_shot(obj, *effect, o, length, opt.force);
}

void stop(lv_obj_t *obj, EffectRef effect) {
  if (obj == nullptr || effect == nullptr) return;
  Slot *slot = detail::find_slot(obj);
  if (slot == nullptr) return;
  WatchGuard watch(*slot);
  if (slot->one_shot.kind == InstanceKind::EFFECT && slot->one_shot.def == effect) stop_one_shot(*slot);
  if ((slot->flags & SLOT_LOOP_ON) && slot->loop.def == effect) clear_loop(*slot);
  if (slot->deferred == effect) slot->deferred = nullptr;
}

}  // namespace lvgl_fx
}  // namespace esphome
