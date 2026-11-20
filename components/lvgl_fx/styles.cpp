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
//   text typewriter / decode Slot::style  TYPEWRITER / DECODE: no offset; the lv_anim callback writes the label text
//                                         from a text run (a fixed record in a static pool, see TextRun)
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

// ---- typewriter / decode: text runs ----
//
// While one of these runs the label shows intermediate text, so the final text has to live somewhere else: in a
// TextRun, a fixed record of a static pool (LVGL_FX_TEXT_RUNS records of LVGL_FX_TEXT_RUN_BYTES bytes, BSS, no heap; YAML `max_text_runs` / `max_text_length`).
// A run belongs to the slot whose Slot::style points at it and is free as soon as that stops being true, so every way
// a style can end (completion, replacement, instant application, target deletion) frees it without any bookkeeping.
// A text of LVGL_FX_TEXT_RUN_BYTES bytes or more, or a full pool, is applied like an instant change (the final text
// is always exact).  Each visible change is one lv_label_set_text, whose copy LVGL keeps in its own label buffer, as
// for `count`.
#ifndef LVGL_FX_TEXT_RUNS
#define LVGL_FX_TEXT_RUNS 4
#endif
#ifndef LVGL_FX_TEXT_RUN_BYTES
#define LVGL_FX_TEXT_RUN_BYTES 128
#endif
constexpr size_t TEXT_RUNS = LVGL_FX_TEXT_RUNS;
constexpr size_t RUN_BYTES = LVGL_FX_TEXT_RUN_BYTES;  // including the terminating NUL
static_assert(TEXT_RUNS >= 1 && RUN_BYTES >= 8 && RUN_BYTES <= 400, "LVGL_FX_TEXT_RUNS / LVGL_FX_TEXT_RUN_BYTES");
constexpr size_t MAX_NOISE = 32;                     // bytes of the decode charset that are used
constexpr const char *DEFAULT_NOISE = "#%&*+<>/=0123456789";
constexpr uint32_t MAX_TYPED_MS = 60000;             // like every duration: char_time * characters is cut here
constexpr uint16_t NOTHING_SHOWN = 0xFFFF;

struct TextRun {
  Slot *owner;           // the slot whose style plays this run (see run_live)
  const char *noise;     // decode: charset (flash), noise_len bytes of it
  uint32_t t0;           // decode: lv_tick of the first frame, the clock of the noise
  uint32_t last_tick;    // decode: noise tick of the text on screen
  uint16_t chars;        // code points of `text`
  uint16_t total;        // value the lv_anim ends at: sum of the character weights (typewriter), ms (decode)
  uint16_t period;       // decode: ms per noise tick
  uint16_t shown;        // typewriter: characters on screen, decode: characters locked (NOTHING_SHOWN: no frame yet)
  uint8_t noise_len;
  bool jitter;
  bool started;          // decode: t0 is set
  char text[RUN_BYTES];  // the final text
};

TextRun g_runs[TEXT_RUNS];
char g_scratch[RUN_BYTES];  // the partial text of one frame, built and handed to LVGL within a single call

bool is_typed(InstanceKind kind) { return kind == InstanceKind::TYPEWRITER || kind == InstanceKind::DECODE; }

bool run_live(const TextRun &run) {
  const Slot *slot = run.owner;
  return slot != nullptr && slot->obj != nullptr && is_typed(slot->style.kind) && slot->style.run == &run;
}

// The run a slot's style plays now, or nullptr.
TextRun *run_of(const Slot *slot) {
  return slot != nullptr && is_typed(slot->style.kind) ? static_cast<TextRun *>(slot->style.run) : nullptr;
}

// The slot's own run (a retarget reuses it), else a free one.
TextRun *take_run(Slot *slot) {
  if (TextRun *own_run = run_of(slot)) return own_run;
  for (TextRun &run : g_runs) {
    if (!run_live(run)) return &run;
  }
  return nullptr;
}

// ---- UTF-8: characters are code points; a sequence is never cut ----

// Length of the sequence at `p` (a NUL ends it): 1 - 4.  A stray continuation byte, an invalid lead byte or a
// truncated sequence counts as one character of one byte, so any text terminates and valid text is never split.
size_t utf8_len(const char *p) {
  const uint8_t lead = static_cast<uint8_t>(*p);
  const size_t n = lead < 0x80 ? 1 : (lead >= 0xC2 && lead <= 0xDF) ? 2 : (lead >= 0xE0 && lead <= 0xEF) ? 3
                   : (lead >= 0xF0 && lead <= 0xF4) ? 4 : 1;
  for (size_t i = 1; i < n; i++) {
    if ((static_cast<uint8_t>(p[i]) & 0xC0) != 0x80) return 1;
  }
  return n;
}

uint32_t code_point(const char *p, size_t n) {
  const uint32_t b0 = static_cast<uint8_t>(p[0]);
  switch (n) {
    case 2:
      return ((b0 & 0x1F) << 6) | (static_cast<uint8_t>(p[1]) & 0x3F);
    case 3:
      return ((b0 & 0x0F) << 12) | ((static_cast<uint8_t>(p[1]) & 0x3F) << 6) | (static_cast<uint8_t>(p[2]) & 0x3F);
    case 4:
      return ((b0 & 0x07) << 18) | ((static_cast<uint8_t>(p[1]) & 0x3F) << 12) |
             ((static_cast<uint8_t>(p[2]) & 0x3F) << 6) | (static_cast<uint8_t>(p[3]) & 0x3F);
    default:
      return b0;
  }
}

// Relative time a typewriter spends on one character with `jitter`: a fixed pseudo-random function of the character
// and its position (a space is quick), so the same text always types with the same rhythm.
uint32_t char_weight(uint32_t cp, uint32_t index) {
  return cp == ' ' ? 45 : 60 + ((cp * 37 + index * 11) % 9) * 10;
}

// Fill a run from `text` (shorter than RUN_BYTES).
void fill_run(TextRun &run, Slot *slot, const char *text, const ResolvedTransition &t, bool decode,
              uint32_t duration) {
  run = TextRun{};
  run.owner = slot;
  run.shown = NOTHING_SHOWN;
  run.jitter = !decode && t.jitter;
  std::strcpy(run.text, text);
  uint32_t total = 0;
  for (const char *p = text; *p != '\0';) {
    const size_t n = utf8_len(p);
    total += run.jitter ? char_weight(code_point(p, n), run.chars) : 1;
    run.chars++;
    p += n;
  }
  run.total = static_cast<uint16_t>(total);  // at most 127 characters of weight 140
  if (decode) {
    // decode animates time in ms, not the locked count: see show_decode
    run.total = static_cast<uint16_t>(std::clamp<uint32_t>(duration, 1, MAX_TYPED_MS));
    size_t noise_len = 0;
    if (t.noise != nullptr) {
      while (noise_len < MAX_NOISE && t.noise[noise_len] >= 0x20 && t.noise[noise_len] < 0x7F) noise_len++;
    }
    run.noise = noise_len != 0 ? t.noise : DEFAULT_NOISE;
    run.noise_len = static_cast<uint8_t>(noise_len != 0 ? noise_len : std::strlen(DEFAULT_NOISE));
    run.period = std::max<uint16_t>(t.noise_period, 1);
  }
}

// Number of code points in a text.
size_t count_chars(const char *text) {
  size_t chars = 0;
  for (const char *p = text; *p != '\0'; p += utf8_len(p)) chars++;
  return chars;
}

// Hand `text` to the label unless it shows it already.
void put_text(lv_obj_t *obj, const char *text) {
  const char *current = lv_label_get_text(obj);
  if (current == nullptr || std::strcmp(current, text) != 0) lv_label_set_text(obj, text);
}

// typewriter: `v` (0 .. total) is time in character weights; the characters whose cumulative weight has passed show.
// The label only changes when a weight threshold is crossed, i.e. when `v` changes, so LVGL calling the exec_cb only on
// a changed path value (lv_anim.c, anim_timer) loses nothing here.
void show_typewriter(Slot *slot, TextRun &run, int32_t v) {
  uint32_t shown = 0, bytes = 0, passed = 0;
  if (v >= run.total) {
    shown = run.chars;
    bytes = std::strlen(run.text);
  } else {
    while (run.text[bytes] != '\0') {
      const size_t n = utf8_len(run.text + bytes);
      passed += run.jitter ? char_weight(code_point(run.text + bytes, n), shown) : 1;
      if (passed > static_cast<uint32_t>(std::max<int32_t>(v, 0))) break;
      shown++;
      bytes += static_cast<uint32_t>(n);
    }
  }
  if (shown == run.shown) return;
  run.shown = static_cast<uint16_t>(shown);
  if (shown >= run.chars) {
    put_text(slot->obj, run.text);
    return;
  }
  std::memcpy(g_scratch, run.text, bytes);
  g_scratch[bytes] = '\0';
  put_text(slot->obj, g_scratch);
}

// decode: the first `locked` positions show the real character; every other printable ASCII character shows a noise
// symbol that changes every `period` ms.  Spaces, control characters and multi-byte characters are never scrambled.
//
// `v` is 0 .. total in ms (already shaped by the curve), not the locked count: LVGL (lv_anim.c, anim_timer) calls an
// animation's exec_cb only when the path value differs from the last one.  Animating the count would run this only
// when one more character locks and the noise would freeze in between; with a value that changes on every frame the
// noise refreshes every `period`, on the lv_tick clock below.  The lock count is derived from the shaped time.
void show_decode(Slot *slot, TextRun &run, int32_t v) {
  const uint32_t clamped = static_cast<uint32_t>(std::clamp<int32_t>(v, 0, run.total));
  const uint32_t locked = static_cast<uint32_t>(static_cast<uint64_t>(clamped) * run.chars / run.total);
  if (!run.started) {
    run.started = true;
    run.t0 = lv_tick_get();
  }
  const uint32_t tick = lv_tick_elaps(run.t0) / run.period;
  if (locked >= run.chars) {
    if (run.shown != run.chars) put_text(slot->obj, run.text);
    run.shown = run.chars;
    return;
  }
  if (locked == run.shown && tick == run.last_tick) return;
  run.shown = static_cast<uint16_t>(locked);
  run.last_tick = tick;
  size_t out = 0;
  uint32_t k = 0;
  for (const char *p = run.text; *p != '\0'; k++) {
    const size_t n = utf8_len(p);
    const uint8_t c = static_cast<uint8_t>(*p);
    if (k >= locked && n == 1 && c > 0x20 && c < 0x7F) {
      g_scratch[out++] = run.noise[(k * 7 + tick * 13 + static_cast<uint32_t>(run.chars) * 5) % run.noise_len];
    } else {
      std::memcpy(g_scratch + out, p, n);
      out += n;
    }
    p += n;
  }
  g_scratch[out] = '\0';
  put_text(slot->obj, g_scratch);
}

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
  if (TextRun *run = run_of(slot)) {
    if (slot->style.kind == InstanceKind::DECODE)
      show_decode(slot, *run, v);
    else
      show_typewriter(slot, *run, v);
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
  auto *slot = static_cast<Slot *>(anim->var);
  if (const TextRun *run = run_of(slot)) put_text(slot->obj, run->text);  // the end value was applied; make sure
  clear_text_style(slot);
  slot_done(anim);
}

// Start the text animation.  Always delete the running one first: lv_anim_start only replaces a same-key animation
// that already applied itself, so a delayed one (early_apply off, still waiting) would survive and later delete the new
// one and drive its instance.
bool start_text_anim(Slot *slot, int32_t from, int32_t to, const ResolvedTransition &t, const Opt &opt,
                     bool early_apply) {
  lv_anim_delete(slot, exec_text);
  return detail::start_transition_anim(slot, exec_text, from, to, t, opt, text_done, early_apply);
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

// The label already shows the requested text: a style that is still writing the label (count, typewriter, decode)
// ends here, since its end would overwrite this.
void cancel_text_writer(Slot *slot) {
  if (slot == nullptr || (slot->style.kind != InstanceKind::COUNT && !is_typed(slot->style.kind))) return;
  lv_anim_delete(slot, exec_text);
  clear_text_style(slot);
}

void text_instant(lv_obj_t *obj, const char *text, const Opt &opt) {
  const char *current = lv_label_get_text(obj);
  if (current != nullptr && std::strcmp(current, text) == 0 && !opt.instant) {
    cancel_text_writer(find_slot(obj));
    return;
  }
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
    return;
  }
  detail::notify_start(obj, opt);
}

// `typewriter` / `decode`: the label does not get the new text first: it shows what the style makes of it, and the
// final text only at the end.  Whatever keeps the text from being typed (no motion allowed, `duration` 0, an empty or
// too long text, no free slot or text run) applies it like any text change: exactly and at once.  A running
// animation of this label (any style) is replaced and the new text starts from scratch.
void typed_to(lv_obj_t *obj, const char *text, const ResolvedTransition &t, const Opt &opt) {
  const bool decode = t.style == Style::DECODE;
  Slot *slot = find_slot(obj);
  const size_t bytes = std::strlen(text);
  const size_t chars = count_chars(text);
  ResolvedTransition timing = t;
  if (t.char_time != 0) timing.duration = static_cast<uint32_t>(std::min<size_t>(chars * t.char_time, MAX_TYPED_MS));
  if (bytes >= RUN_BYTES)
    detail::warn_once(2, "typewriter/decode: text too long (LVGL_FX_TEXT_RUN_BYTES), applied instantly");
  const bool typed = bytes != 0 && bytes < RUN_BYTES && timing.duration != 0 && transition_allowed(obj, true, opt);
  if (typed && slot == nullptr) slot = acquire_slot(obj);
  TextRun *run = typed && slot != nullptr ? take_run(slot) : nullptr;
  if (typed && slot != nullptr && run == nullptr)
    detail::warn_once(3, "typewriter/decode: all text runs are busy (LVGL_FX_TEXT_RUNS), applied instantly");
  if (run == nullptr) {
    put_text(obj, text);
    if (slot != nullptr)
      apply_instantly(obj, slot, opt);
    else
      detail::instant_then(obj, opt);
    return;
  }

  fill_run(*run, slot, text, t, decode, timing.duration);
  Instance entry{};
  entry.run = run;
  entry.kind = decode ? InstanceKind::DECODE : InstanceKind::TYPEWRITER;
  set_text_style(slot, entry);
  // During `delay` the label keeps its old text: the first frame is applied when the motion starts.
  if (!start_text_anim(slot, 0, run->total, timing, opt, t.delay == 0)) {
    clear_text_style(slot);
    put_text(obj, text);
    detail::instant_then(obj, opt);
    return;
  }
  detail::notify_start(obj, opt);
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
  // A typewriter / decode already on its way to this very text: nothing to do (it keeps running).
  const TextRun *typing = run_of(find_slot(obj));
  if (typing != nullptr && std::strcmp(typing->text, text) == 0) return;
  if (current != nullptr && std::strcmp(current, text) == 0) {
    // Already showing it.  A count / typewriter / decode still running towards another text ends here (its end would
    // overwrite this).
    cancel_text_writer(find_slot(obj));
    return;
  }

  const ResolvedTransition t = resolve(opt.transition, Prop::TEXT);
  if (t.style == Style::TYPEWRITER || t.style == Style::DECODE) {
    typed_to(obj, text, t, opt);
    return;
  }
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
    return;
  }
  detail::notify_start(obj, opt);
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
  // The style instance also holds the draw state of an lv_line (draw.cpp): only text styles are cleared here.
  const InstanceKind kind = slot->style.kind;
  if (kind != InstanceKind::LINE_RUN && kind != InstanceKind::LINE_REST && kind != InstanceKind::NONE)
    clear_text_style(slot);
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
