#include "lvgl_fx.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

#include "esphome/core/log.h"

namespace esphome {
namespace lvgl_fx {

// Line draw: `set_draw(line, percent)` draws or erases an lv_line along its points, by path length.
//
// LVGL draws exactly the point array it is given, so a partial line is a partial array.  The full array is the app's
// (LVGL's rule: it must outlive the line); fx captures it from the line the first time it is drawn and only ever hands
// it back.  The interpolated partial array lives in a LineRun, a fixed record of a static pool (the typewriter /
// decode text runs of styles.cpp are the model): LVGL_FX_LINE_RUNS records of LVGL_FX_LINE_POINTS points, BSS, no heap.
//
// State of a line, kept in Slot::style (a line has no text style, so the instance is free):
//   no instance     the line shows its own full array: 100 %.  Nothing is stored, nothing to free.
//   LINE_RUN        the line shows run->partial (0 < p < 100, or while it animates).  The run is bound to the slot.
//   LINE_REST       at rest without a run: 0 % (the line shows `full` with 1 point, which LVGL draws as nothing), or a
//                   degraded partial (pool busy / too many points: whole segments only).  full / count / percent
//                   live in the instance, so a hidden, erased line costs a slot and no run.
// A run is free as soon as no slot's style points at it (run_live), so completion, replacement, instant application and
// target deletion free it without any bookkeeping.
//
// The line is "ours" while its points pointer is the run's partial buffer or the captured full array.  Anything else
// means the app called lv_line_set_points with another array (for ESPHome's own `line` widget, `lvgl.line.update`):
// fx forgets its record and captures the new array as the full line.  Only pointers are compared until ownership is
// established, never the old array (it may be freed already).

#if LV_USE_LINE

#ifndef LVGL_FX_LINE_RUNS
#define LVGL_FX_LINE_RUNS 4
#endif
#ifndef LVGL_FX_LINE_POINTS
#define LVGL_FX_LINE_POINTS 16
#endif

namespace {

using detail::acquire_slot;
using detail::find_slot;
using detail::resolve;
using detail::ResolvedTransition;
using detail::transition_allowed;

constexpr size_t LINE_RUNS = LVGL_FX_LINE_RUNS;
constexpr size_t RUN_POINTS = LVGL_FX_LINE_POINTS;
static_assert(LINE_RUNS >= 1 && RUN_POINTS >= 2 && RUN_POINTS <= 255, "LVGL_FX_LINE_RUNS / LVGL_FX_LINE_POINTS");

constexpr int32_t FULL = 10000;              // 100.00 %: the animated value is in 1/100 %
constexpr uint32_t MAX_FULL_POINTS = 255;    // LINE_REST keeps the point count in the 8-bit `state`
constexpr uint16_t PERCENT_MASK = 0x3FFF;    // LINE_REST progress: percent in 1/100 %
constexpr uint16_t MUTABLE_FLAG = 0x8000;    // LINE_REST progress: the full array was set with lv_line_set_points_mutable

struct LineRun {
  Slot *owner;                          // the slot whose style plays this run (see run_live)
  const lv_point_precise_t *full;       // the app's array
  int32_t shown;                        // percent (1/100 %) on screen now
  uint16_t count;                       // points of `full`
  bool full_mutable;
  lv_point_precise_t partial[RUN_POINTS];
};

LineRun g_runs[LINE_RUNS];

// What a line is, read back from the slot (or from the line itself when fx has not touched it yet).
struct Capture {
  const lv_point_precise_t *full;
  uint16_t count;
  bool full_mutable;
  int32_t shown;
  LineRun *run;  // nullptr: at rest without a run
};

bool run_live(const LineRun &run) {
  const Slot *slot = run.owner;
  return slot != nullptr && slot->obj != nullptr && slot->style.kind == InstanceKind::LINE_RUN &&
         slot->style.run == &run;
}

LineRun *run_of(const Slot *slot) {
  return slot != nullptr && slot->style.kind == InstanceKind::LINE_RUN ? static_cast<LineRun *>(slot->style.run)
                                                                       : nullptr;
}

// The slot's own run (a retarget reuses it), else a free one.
LineRun *take_run(Slot *slot) {
  if (LineRun *own_run = run_of(slot)) return own_run;
  for (LineRun &run : g_runs) {
    if (!run_live(run)) return &run;
  }
  return nullptr;
}

// ---- geometry ----

// Coordinates may be percentages of the line's size (ESPHome `x: 50%`): resolved like LVGL does when it draws.
struct Extent {
  lv_obj_t *obj;
  int32_t w = -1;
  int32_t h = -1;
  float resolve(lv_value_precise_t v, bool horizontal) {
    if (!LV_COORD_IS_PCT(static_cast<int32_t>(v))) return static_cast<float>(v);
    int32_t &size = horizontal ? w : h;
    if (size < 0) size = horizontal ? lv_obj_get_width(obj) : lv_obj_get_height(obj);
    return static_cast<float>(LV_CLAMP(0, size * LV_COORD_GET_PCT(static_cast<int32_t>(v)) / 100, size));
  }
};

lv_value_precise_t to_precise(float v) {
  if constexpr (std::is_floating_point<lv_value_precise_t>::value)
    return static_cast<lv_value_precise_t>(v);
  else
    return static_cast<lv_value_precise_t>(std::lround(v));
}

float segment_length(Extent &extent, const lv_point_precise_t &a, const lv_point_precise_t &b) {
  return std::hypot(extent.resolve(b.x, true) - extent.resolve(a.x, true),
                    extent.resolve(b.y, false) - extent.resolve(a.y, false));
}

float path_length(lv_obj_t *line, const lv_point_precise_t *full, uint32_t count) {
  Extent extent{line};
  float total = 0.0f;
  for (uint32_t i = 1; i < count; i++) total += segment_length(extent, full[i - 1], full[i]);
  return total;
}

// Points of the line drawn up to `v` (0 < v < FULL), the last one interpolated along its segment.  The result is
// written to `out` (RUN_POINTS entries at least `count`); returns how many points to show.
uint32_t fill_partial(lv_obj_t *line, const lv_point_precise_t *full, uint32_t count, int32_t v,
                      lv_point_precise_t *out) {
  Extent extent{line};
  float remaining = path_length(line, full, count) * static_cast<float>(v) / static_cast<float>(FULL);
  out[0] = full[0];
  for (uint32_t i = 1; i < count; i++) {
    const float length = segment_length(extent, full[i - 1], full[i]);
    if (remaining >= length) {
      out[i] = full[i];
      remaining -= length;
      continue;
    }
    const float f = length > 0.0f ? remaining / length : 0.0f;
    const float ax = extent.resolve(full[i - 1].x, true), ay = extent.resolve(full[i - 1].y, false);
    const float bx = extent.resolve(full[i].x, true), by = extent.resolve(full[i].y, false);
    out[i].x = to_precise(ax + (bx - ax) * f);
    out[i].y = to_precise(ay + (by - ay) * f);
    return i + 1;
  }
  return count;
}

// Without a buffer: the whole segments that fit into `v` (1 point = nothing drawn).
uint32_t whole_points(lv_obj_t *line, const lv_point_precise_t *full, uint32_t count, int32_t v) {
  if (v <= 0) return 1;
  if (v >= FULL) return count;
  Extent extent{line};
  float remaining = path_length(line, full, count) * static_cast<float>(v) / static_cast<float>(FULL);
  uint32_t shown = 1;
  for (uint32_t i = 1; i < count; i++) {
    const float length = segment_length(extent, full[i - 1], full[i]);
    if (remaining < length) break;
    remaining -= length;
    shown = i + 1;
  }
  return shown;
}

// ---- state ----

uint32_t rest_points(lv_obj_t *line, const Capture &c) { return whole_points(line, c.full, c.count, c.shown); }

Instance rest_instance(const Capture &c, int32_t shown) {
  Instance rest{};
  rest.def = c.full;
  rest.kind = InstanceKind::LINE_REST;
  rest.state = static_cast<uint8_t>(c.count);
  rest.progress = static_cast<uint16_t>((shown & PERCENT_MASK) | (c.full_mutable ? MUTABLE_FLAG : 0));
  return rest;
}

// Put `v` on the line.  `run`: the buffer for a partial line, nullptr to show whole segments only.
void show(lv_obj_t *line, const Capture &c, int32_t v, LineRun *run) {
  if (v >= FULL) {
    if (c.full_mutable)
      lv_line_set_points_mutable(line, const_cast<lv_point_precise_t *>(c.full), c.count);
    else
      lv_line_set_points(line, c.full, c.count);
  } else if (v <= 0) {
    lv_line_set_points(line, c.full, 1);  // LVGL draws nothing for fewer than 2 points
  } else if (run != nullptr) {
    const uint32_t shown = fill_partial(line, c.full, c.count, v, run->partial);
    lv_line_set_points(line, run->partial, shown);
  } else {
    lv_line_set_points(line, c.full, whole_points(line, c.full, c.count, v));
  }
}

void exec_draw(void *var, int32_t v);

void forget_state(Slot *slot) {
  lv_anim_delete(slot, exec_draw);
  slot->style = Instance{};
}

Capture capture_of(const LineRun &run) {
  return {run.full, run.count, run.full_mutable, run.shown, const_cast<LineRun *>(&run)};
}

// What the line is now.  False: there is nothing to draw (fewer than 2 points) or the array is not supported.
bool read_state(lv_obj_t *line, Slot *slot, Capture &c) {
  const lv_point_precise_t *current = lv_line_get_points(line);
  const uint32_t count = lv_line_get_point_count(line);
  if (slot != nullptr) {
    if (LineRun *run = run_of(slot)) {
      // At rest a bound run means a partial line, which shows the partial buffer; while it animates the line may also
      // show the full array (before the first frame, at 0 and at 100 %).
      const bool animating = lv_anim_get(slot, exec_draw) != nullptr;
      if (current == run->partial || (animating && current == run->full)) {
        c = capture_of(*run);
        return true;
      }
      forget_state(slot);  // the app set another array
    } else if (slot->style.kind == InstanceKind::LINE_REST) {
      const auto *full = static_cast<const lv_point_precise_t *>(slot->style.def);
      Capture rest{full, slot->style.state, (slot->style.progress & MUTABLE_FLAG) != 0,
                   static_cast<int32_t>(slot->style.progress & PERCENT_MASK), nullptr};
      if (current == full && count == rest_points(line, rest)) {
        c = rest;
        return true;
      }
      forget_state(slot);
    }
  }
  if (current == nullptr || count < 2) {
    detail::warn_once(6, "set_draw: the line has fewer than 2 points: nothing to draw");
    return false;
  }
  if (count > MAX_FULL_POINTS) {
    detail::warn_once(7, "set_draw: lines of more than 255 points are not supported");
    return false;
  }
  c = {current, static_cast<uint16_t>(count), lv_line_is_point_array_mutable(line), FULL, nullptr};
  return true;
}

void bind_run(LineRun *run, Slot *slot, const Capture &c, int32_t shown) {
  run->owner = slot;
  run->full = c.full;
  run->count = c.count;
  run->full_mutable = c.full_mutable;
  run->shown = shown;
  Instance entry{};
  entry.run = run;
  entry.kind = InstanceKind::LINE_RUN;
  slot->style = entry;
}

// Apply `v` as the resting state, without motion.
void settle(lv_obj_t *line, Slot *slot, const Capture &c, int32_t v) {
  lv_anim_delete(slot, exec_draw);
  if (v >= FULL) {
    show(line, c, FULL, nullptr);
    slot->style = Instance{};  // the line shows its own array again: the run, if any, is free
    return;
  }
  if (v <= 0) {
    show(line, c, 0, nullptr);
    slot->style = rest_instance(c, 0);
    return;
  }
  LineRun *run = c.count <= RUN_POINTS ? take_run(slot) : nullptr;
  if (run == nullptr) {
    detail::warn_once(5, "set_draw: no free line run (LVGL_FX_LINE_RUNS) or too many points (LVGL_FX_LINE_POINTS): "
                         "whole segments only");
    show(line, c, v, nullptr);
    slot->style = rest_instance(c, v);
    return;
  }
  bind_run(run, slot, c, v);
  show(line, c, v, run);
}

// ---- the animation: var = slot, one exec_cb ----

void exec_draw(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  LineRun *run = run_of(slot);
  if (run == nullptr) return;
  const lv_point_precise_t *current = lv_line_get_points(slot->obj);
  if (current != run->partial && current != run->full) {
    forget_state(slot);  // the app took the line over: the animation ends here, the new array stays
    return;
  }
  run->shown = v;
  show(slot->obj, capture_of(*run), v, run);
}

void draw_done(lv_anim_t *anim) {
  auto *slot = static_cast<Slot *>(anim->var);
  LineRun *run = run_of(slot);
  if (run == nullptr) return;  // ended by the app taking the line over: not a completion
  const lv_point_precise_t *current = lv_line_get_points(slot->obj);
  if (current != run->partial && current != run->full) {
    forget_state(slot);
    return;
  }
  const int32_t end = anim->end_value;
  const Capture c = capture_of(*run);
  run->shown = end;
  show(slot->obj, c, end, run);
  if (end >= FULL) {
    slot->style = Instance{};
  } else if (end <= 0) {
    slot->style = rest_instance(c, 0);
  }  // partial: the run stays bound
  detail::slot_done(anim);
}

}  // namespace

void set_draw(lv_obj_t *obj, float percent, Opt opt) {
  if (obj == nullptr) return;
  opt = select_transition(obj, Prop::DRAW, opt);
  if (!lv_obj_check_type(obj, &lv_line_class)) {
    detail::warn_once(4, "set_draw: target is not a line");
    return;
  }
  const bool finite = std::isfinite(percent);
  const int32_t target = finite ? static_cast<int32_t>(std::lround(std::clamp(percent, 0.0f, 100.0f) * 100.0f)) : 0;
  Slot *slot = find_slot(obj);
  Capture c;
  if (!read_state(obj, slot, c)) return;

  const lv_anim_t *running = slot != nullptr ? lv_anim_get(slot, exec_draw) : nullptr;
  const int32_t heading = running != nullptr ? running->end_value : c.shown;
  // Already there, or already on its way there: nothing to do (a running draw keeps going).
  if (!opt.instant && target == heading) return;
  if (opt.instant && running == nullptr && target == c.shown) return;
  if (slot == nullptr) slot = acquire_slot(obj);
  if (slot == nullptr) {  // the state of a line that is not fully drawn needs a slot
    detail::warn_once(8, "set_draw: no free target slot (max_targets): the line is left as it is");
    return;
  }

  const ResolvedTransition t = resolve(opt.transition, Prop::DRAW);
  const bool animate = !opt.instant && finite && t.duration != 0 && c.shown != target && transition_allowed(obj, true, opt);
  LineRun *run = nullptr;
  if (animate) {
    run = c.count <= RUN_POINTS ? take_run(slot) : nullptr;
    if (run == nullptr)
      detail::warn_once(5, "set_draw: no free line run (LVGL_FX_LINE_RUNS) or too many points (LVGL_FX_LINE_POINTS): "
                           "applied without motion, whole segments only");
  }
  if (run == nullptr) {
    settle(obj, slot, c, target);
    if (c.shown != target) detail::instant_then(obj, opt);
    return;
  }
  // Motion from what is on screen (a retarget continues from there).  During `delay` the line keeps what it shows.
  lv_anim_delete(slot, exec_draw);
  bind_run(run, slot, c, c.shown);
  if (!detail::start_transition_anim(slot, exec_draw, c.shown, target, t, opt, draw_done, t.delay == 0)) {
    settle(obj, slot, c, target);
    detail::instant_then(obj, opt);
    return;
  }
  detail::notify_start(obj, opt);
}

namespace detail {
void stop_draw(Slot *slot) { snap(slot, exec_draw); }
}  // namespace detail

#else  // !LV_USE_LINE

void set_draw(lv_obj_t *obj, float percent, Opt opt) {
  (void) obj;
  (void) percent;
  (void) opt;
  detail::warn_once(4, "set_draw: LVGL line support is disabled");
}

namespace detail {
void stop_draw(Slot *) {}
}  // namespace detail

#endif  // LV_USE_LINE

}  // namespace lvgl_fx
}  // namespace esphome
