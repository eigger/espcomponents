#include "lvgl_fx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome {
namespace lvgl_fx {

// Transition styles.  A style is "base value change + an offset that returns to 0": it never writes
// a widget property of its own, it only supplies an Instance whose contribution props.cpp merges:
//
//   visible slide_* / zoom   Slot::vis    offset derived from the base opacity the visible transition animates
//   text fade / roll         Slot::style  TEXT_ENTER: progress 0..255 of the lv_anim (opacity + vertical travel)
//   text count               Slot::style  COUNT: no offset; the lv_anim callback writes the label text
//
// All frame callbacks use integers, except count, which formats one number when the displayed value changes.

using detail::acquire_slot;
using detail::find_slot;
using detail::resolve;
using detail::ResolvedTransition;
using detail::slot_done;
using detail::start_anim;
using detail::then_of;
using detail::transition_allowed;

constexpr uint32_t MAX_DISTANCE = 1000;  // px: keeps every offset sum inside int16

// ---- visible styles ----------------------------------------------------------------------------------------------

namespace detail {

void begin_visible_style(Slot *slot, Style style, const ResolvedTransition &t, bool in_flight) {
  if (in_flight) return;  // a reversal keeps the style it started with: the offsets retrace their path, no jump
  drop_visible_style(slot);
  Instance entry{};
  switch (style) {
    case Style::SLIDE_UP:
      entry.kind = InstanceKind::SLIDE_UP;
      break;
    case Style::SLIDE_DOWN:
      entry.kind = InstanceKind::SLIDE_DOWN;
      break;
    case Style::SLIDE_LEFT:
      entry.kind = InstanceKind::SLIDE_LEFT;
      break;
    case Style::SLIDE_RIGHT:
      entry.kind = InstanceKind::SLIDE_RIGHT;
      break;
    case Style::ZOOM:
      entry.kind = InstanceKind::ZOOM;
      break;
    default:  // fade (opacity only) and none (never gets here) have no offset
      return;
  }
  if (entry.kind == InstanceKind::ZOOM) {
    own(*slot, PROP_SCALE);
    entry.arg.a = static_cast<int16_t>(std::min<uint32_t>(t.from, 200) * LV_SCALE_NONE / 100);
  } else {
    own(*slot, PROP_TRANSLATE);
    entry.arg.a = static_cast<int16_t>(std::min<uint32_t>(t.distance, MAX_DISTANCE));
  }
  entry.state = slot->visible_opa;  // offset 0 at this opacity, full offset at 0
  slot->vis = entry;
}

void drop_visible_style(Slot *slot) {
  if (slot->vis.kind == InstanceKind::NONE) return;
  const uint8_t offsets = instance_props(slot->vis);
  slot->vis = Instance{};
  compose(*slot, offsets);
}

}  // namespace detail

// ---- text styles -------------------------------------------------------------------------------------------------

#if LV_USE_LABEL

namespace {

constexpr size_t TEXT_BUF = 48;       // formatted numbers: longer text is cut
constexpr uint8_t MAX_DECIMALS = 6;   // count runs on fixed-point lv_anim values: value * 10^decimals
constexpr double POW10[MAX_DECIMALS + 1] = {1.0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6};
constexpr double FIXED_LIMIT = 2.0e9;  // stay inside int32
// lv_anim paths compute step (0..1024) * (end - start) in int32: a count must not span more than this.
constexpr int64_t MAX_COUNT_DELTA = (INT32_MAX >> 10) - 1;

// Leading number of a text: optional sign, digits, optional fraction.  No exponent, no hex, no inf/nan.
bool parse_number(const char *text, double *out) {
  if (text == nullptr) return false;
  const char *p = text;
  while (*p == ' ') p++;  // width-padded formats
  bool negative = false;
  if (*p == '-' || *p == '+') negative = *p++ == '-';
  double value = 0.0;
  bool digits = false;
  for (; *p >= '0' && *p <= '9'; p++, digits = true) value = value * 10.0 + (*p - '0');
  if (*p == '.') {
    double place = 0.1;
    for (p++; *p >= '0' && *p <= '9'; p++, digits = true, place *= 0.1) value += (*p - '0') * place;
  }
  if (!digits || !std::isfinite(value)) return false;
  *out = negative ? -value : value;
  return true;
}

// Characters of `format` before its first conversion: a label shows that literal prefix before the number.
size_t format_prefix_len(const char *format) {
  const char *percent = std::strchr(format, '%');
  return percent != nullptr ? static_cast<size_t>(percent - format) : std::strlen(format);
}

// Decimals of an %f conversion (the only kind count can move through fixed-point numbers without changing the final
// text); -1 for any other format.
int format_decimals(const char *format) {
  const char *p = std::strchr(format, '%');
  while (p != nullptr && p[1] == '%') p = std::strchr(p + 2, '%');
  if (p == nullptr) return -1;
  p++;
  while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') p++;  // flags
  while (*p >= '0' && *p <= '9') p++;                                          // width
  int precision = 6;
  if (*p == '.') {
    precision = 0;
    for (p++; *p >= '0' && *p <= '9'; p++) precision = std::min(precision * 10 + (*p - '0'), 99);
  }
  return (*p == 'f' || *p == 'F') && precision <= MAX_DECIMALS ? precision : -1;
}

// Round half to even like printf, so the last count step shows the same text a plain format would.
int32_t to_fixed(double value, uint8_t decimals) {
  return static_cast<int32_t>(std::lrint(std::clamp(value * POW10[decimals], -FIXED_LIMIT, FIXED_LIMIT)));
}

// ---- the lv_anim of every text style: var = slot, one exec_cb (so a new style replaces the running one) ----

void show_count(Slot *slot, int32_t v) {
  char text[TEXT_BUF];
  const char *format = slot->style.format != nullptr ? slot->style.format : "%.0f";
  const double number = static_cast<double>(v) / POW10[std::min<uint8_t>(slot->style.state, MAX_DECIMALS)];
  std::snprintf(text, sizeof(text), format, number);
  const char *current = lv_label_get_text(slot->obj);
  if (current == nullptr || std::strcmp(current, text) != 0) lv_label_set_text(slot->obj, text);
}

void exec_text(void *var, int32_t v) {
  auto *slot = static_cast<Slot *>(var);
  if (slot->style.kind == InstanceKind::COUNT) {
    show_count(slot, v);
    return;
  }
  slot->style.progress = static_cast<uint16_t>(std::clamp<int32_t>(v, 0, LV_OPA_COVER));
  compose(*slot, instance_props(slot->style));
}

// Offsets back to 0, whatever the style was.
void clear_text_style(Slot *slot) {
  const uint8_t offsets = instance_props(slot->style);
  slot->style = Instance{};
  compose(*slot, offsets);
}

// A new style replaces the old one: whatever only the old one moved goes back to rest.
void set_text_style(Slot *slot, const Instance &next) {
  const uint8_t previous = instance_props(slot->style);
  slot->style = next;
  compose(*slot, previous);
}

void text_done(lv_anim_t *anim) {
  clear_text_style(static_cast<Slot *>(anim->var));
  slot_done(anim);
}

// Start the text animation.  Always delete the running one first: lv_anim_start only replaces a same-key animation
// that already applied itself, so a delayed one (early_apply off, still waiting) would survive and later delete the new
// one and drive its instance.
bool start_text_anim(Slot *slot, int32_t from, int32_t to, const ResolvedTransition &t, const Opt &opt,
                     bool early_apply) {
  lv_anim_delete(slot, exec_text);
  return start_anim(slot, exec_text, from, to, t.duration, t.delay, t.curve, text_done, then_of(opt), early_apply);
}

// The new text is already in the label (the state is exact first, the motion is decoration); this ends the motion of
// an earlier text change when the new one is applied instantly.
void apply_instantly(lv_obj_t *obj, Slot *slot, const Opt &opt) {
  if (slot != nullptr) {
    lv_anim_delete(slot, exec_text);
    clear_text_style(slot);
  }
  detail::instant_then(obj, opt);
}

void text_instant(lv_obj_t *obj, const char *text, const Opt &opt) {
  const char *current = lv_label_get_text(obj);
  if (current != nullptr && std::strcmp(current, text) == 0 && !opt.instant) return;
  lv_label_set_text(obj, text);
  apply_instantly(obj, find_slot(obj), opt);
}

// `count`: the number runs from the one the label shows to `value`.  Without a number to start from (empty or
// text that does not start with the format's literal prefix and a number), without motion, with nothing to count,
// with a format that is not an %f conversion or numbers too large for the fixed-point range, the new text is applied
// like any text change (fade): the final text is always exactly what the format gives for `value`.
void count_to(lv_obj_t *obj, double value, const char *final_text, const ResolvedTransition &t, const Opt &opt) {
  const char *current = lv_label_get_text(obj);
  Slot *slot = find_slot(obj);
  const lv_anim_t *running =
      slot != nullptr && slot->style.kind == InstanceKind::COUNT ? lv_anim_get(slot, exec_text) : nullptr;
  if (running != nullptr) {
    if (running->end_value == to_fixed(value, slot->style.state)) return;  // already counting there
  } else if (current != nullptr && std::strcmp(current, final_text) == 0) {
    return;
  }

  double before = 0.0;
  const size_t prefix = format_prefix_len(t.format);
  const bool have_before = current != nullptr && std::strncmp(current, t.format, prefix) == 0 &&
                           parse_number(current + prefix, &before);
  const int decimals = have_before ? format_decimals(t.format) : -1;
  const bool fits = decimals >= 0 && std::max(std::fabs(before), std::fabs(value)) *
                                             POW10[decimals] <=
                                         FIXED_LIMIT;
  const int32_t to = fits ? to_fixed(value, static_cast<uint8_t>(decimals)) : 0;
  const int32_t from = fits ? to_fixed(before, static_cast<uint8_t>(decimals)) : 0;
  const bool counting = fits && from != to && std::llabs(static_cast<int64_t>(to) - from) <= MAX_COUNT_DELTA &&
                        t.duration != 0 && transition_allowed(obj, true, opt);
  if (counting && slot == nullptr) slot = acquire_slot(obj);
  if (!counting || slot == nullptr) {
    set_text(obj, final_text, opt);  // the count style falls back to fade there
    return;
  }

  Instance count{};
  count.format = t.format;
  count.kind = InstanceKind::COUNT;
  count.state = static_cast<uint8_t>(decimals);
  set_text_style(slot, count);
  if (!start_text_anim(slot, from, to, t, opt, true)) {
    clear_text_style(slot);
    lv_label_set_text(obj, final_text);
    detail::instant_then(obj, opt);
  }
}

}  // namespace

void set_text(lv_obj_t *obj, const char *text, Opt opt) {
  if (obj == nullptr) return;
  opt = select_transition(obj, Prop::TEXT, opt);
  if (text == nullptr) text = "";
  if (!lv_obj_check_type(obj, &lv_label_class)) {
    detail::warn_once(1, "set_text: target is not a label");
    return;
  }
  const char *current = lv_label_get_text(obj);
  if (opt.instant) {
    if (current == nullptr || std::strcmp(current, text) != 0) lv_label_set_text(obj, text);
    apply_instantly(obj, find_slot(obj), opt);
    return;
  }
  if (current != nullptr && std::strcmp(current, text) == 0) {
    // Already showing it.  A count still running towards another number ends here (its end would overwrite this).
    Slot *counting = find_slot(obj);
    if (counting != nullptr && counting->style.kind == InstanceKind::COUNT) {
      lv_anim_delete(counting, exec_text);
      clear_text_style(counting);
    }
    return;
  }

  const ResolvedTransition t = resolve(opt.transition, Prop::TEXT);
  // roll: from below when the leading number grows, from above when it shrinks; anything else fades.
  int32_t travel = 0;
  if (t.style == Style::ROLL) {
    double before = 0.0, after = 0.0;
    const int32_t distance = static_cast<int32_t>(std::min<uint32_t>(t.distance, MAX_DISTANCE));
    if (current != nullptr && parse_number(current, &before) && parse_number(text, &after))
      travel = after > before ? distance : (after < before ? -distance : 0);
  }
  lv_label_set_text(obj, text);  // the state is exact first; the motion is decoration

  Slot *slot = find_slot(obj);
  const uint8_t from = static_cast<uint8_t>(std::min<uint32_t>(t.from, 100) * LV_OPA_COVER / 100);
  if (t.style == Style::NONE || t.duration == 0 || (travel == 0 && from >= LV_OPA_COVER) ||
      !transition_allowed(obj, true, opt)) {
    apply_instantly(obj, slot, opt);
    return;
  }
  if (slot == nullptr) slot = acquire_slot(obj);
  if (slot == nullptr) {  // pool full: plain text swap
    detail::instant_then(obj, opt);
    return;
  }
  own(*slot, static_cast<uint8_t>(travel != 0 ? (PROP_OPA | PROP_TRANSLATE) : PROP_OPA));
  // During `delay` the new text is shown as it is (the entry offsets are the identity until the motion starts): the
  // animation does not apply its first value early.
  const bool delayed = t.delay != 0;
  Instance enter{};
  enter.kind = InstanceKind::TEXT_ENTER;
  enter.arg.a = static_cast<int16_t>(travel);
  enter.arg.b = from;
  enter.progress = delayed ? LV_OPA_COVER : 0;
  set_text_style(slot, enter);
  if (!start_text_anim(slot, 0, LV_OPA_COVER, t, opt, !delayed)) {
    clear_text_style(slot);
    detail::instant_then(obj, opt);
  }
}

namespace detail {

void set_label_value(lv_obj_t *obj, float value, const Opt &opt) {
  if (!std::isfinite(value)) {
    text_instant(obj, "--", opt);
    return;
  }
  const ResolvedTransition t = resolve(opt.transition, Prop::TEXT);
  char text[TEXT_BUF];
  std::snprintf(text, sizeof(text), t.format, static_cast<double>(value));
  if (t.style == Style::COUNT && !opt.instant)
    count_to(obj, static_cast<double>(value), text, t, opt);
  else
    set_text(obj, text, opt);
}

void stop_text(Slot *slot) {
  snap(slot, exec_text);
  if (slot->style.kind != InstanceKind::NONE) clear_text_style(slot);
}

}  // namespace detail

#else  // !LV_USE_LABEL

void set_text(lv_obj_t *obj, const char *text, Opt opt) {
  (void) obj;
  (void) text;
  (void) opt;
  detail::warn_once(1, "set_text: LVGL label support is disabled");
}

namespace detail {
void set_label_value(lv_obj_t *, float, const Opt &) {}
void stop_text(Slot *) {}
}  // namespace detail

#endif  // LV_USE_LABEL

}  // namespace lvgl_fx
}  // namespace esphome
