#include "lvgl_fx.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome {
namespace lvgl_fx {

static const char *const TAG = "lvgl_fx";

// Track math.  Rules every track follows: input is the definition, the time into the effect
// and the intensity; output is an offset merged into `out`; the widget is never touched; the offset is zero before
// the track's delay and from its end on; integer math only, no allocation.

namespace {

constexpr int32_t ONE = LV_BEZIER_VAL_MAX;  // 1.0 in the curve domain (1024)

// A track with every unset field resolved.  Only the fields of its kind are meaningful.
struct Resolved {
  TrackKind kind;
  Curve curve;
  uint32_t delay;
  uint32_t duration;
  int32_t scale_delta_q8;  // pulse: peak scale - 100%, Q8 (256 = 100%)
  int32_t x, y;            // nudge: px; shake: x = amplitude, y unused
  uint8_t cycles;
  Axis axis;
  FlashProp property;
  FlashProfile profile;
  lv_color_t color;
  uint8_t amount;          // flash strength / glow peak outline opacity / blink lowest opacity, 0..255
};

const Track NO_PRESET = {TrackKind::PULSE};

// preset field, then the track's own, then the built-in value.
template <typename T> inline T pick(T preset, T own, T unset, T builtin) {
  if (preset != unset) return preset;
  if (own != unset) return own;
  return builtin;
}

inline uint8_t percent_to_255(int32_t percent) {
  return static_cast<uint8_t>(std::clamp<int32_t>(percent, 0, 100) * 255 / 100);
}

uint16_t default_duration(TrackKind kind) {
  switch (kind) {
    case TrackKind::SHAKE:
      return DUR_FAST;
    case TrackKind::FLASH:
    case TrackKind::GLOW:
      return DUR_SLOW;
    default:
      return DUR_NORMAL;
  }
}

Resolved resolve_track(const EffectDef &def, const Track &t) {
  const Track *preset = def.builtin != 0 ? detail::core().presets[def.builtin - 1] : nullptr;
  const Track &p = (preset != nullptr && preset->kind == t.kind) ? *preset : NO_PRESET;

  Resolved r{};
  r.kind = t.kind;
  const Curve curve = pick(p.curve, t.curve, Curve::DEFAULT, Curve::LINEAR);
  r.curve = curve;
  r.delay = t.delay;
  r.duration = detail::resolve_duration(pick(p.duration, t.duration, DUR_DEFAULT, default_duration(t.kind)));
  switch (t.kind) {
    case TrackKind::PULSE: {
      const int32_t scale = pick<uint16_t>(p.scale, t.scale, UNSET_U16, 110);
      r.scale_delta_q8 = (scale * LV_SCALE_NONE + 50) / 100 - LV_SCALE_NONE;
      break;
    }
    case TrackKind::NUDGE:
      r.x = pick<int16_t>(p.x, t.x, UNSET_I16, 0);
      r.y = pick<int16_t>(p.y, t.y, UNSET_I16, 0);
      break;
    case TrackKind::SHAKE:
      r.x = pick<uint16_t>(p.distance, t.distance, UNSET_U16, 2);
      r.cycles = static_cast<uint8_t>(pick<uint8_t>(p.cycles, t.cycles, 0, 2));
      r.axis = pick(p.axis, t.axis, Axis::DEFAULT, Axis::X);
      break;
    case TrackKind::FLASH: {
      r.property = pick(p.property, t.property, FlashProp::AUTO, FlashProp::AUTO);
      r.profile = pick(p.profile, t.profile, FlashProfile::DEFAULT, FlashProfile::FADE_OUT);
      const int32_t color = pick<int32_t>(p.color, t.color, UNSET_COLOR, 0xFFFFFF);
      r.color = lv_color_hex(static_cast<uint32_t>(color));
      r.amount = percent_to_255(pick<uint8_t>(p.amount, t.amount, UNSET_U8, 100));
      break;
    }
    case TrackKind::IMAGE_PULSE:
      r.amount = percent_to_255(pick<uint8_t>(p.amount, t.amount, UNSET_U8, 30));
      break;
    case TrackKind::GLOW:
      r.amount = percent_to_255(pick<uint8_t>(p.opacity, t.opacity, UNSET_U8, 50));
      break;
    case TrackKind::BLINK:
      r.amount = percent_to_255(pick<uint8_t>(p.opacity, t.opacity, UNSET_U8, 30));
      break;
    case TrackKind::COUNT:
      break;
  }
  return r;
}

// magnitude * intensity (Q8), kept inside int16 so the products below cannot overflow.
inline int32_t scaled(int32_t magnitude, uint32_t intensity_q8) {
  return std::clamp<int32_t>(magnitude * static_cast<int32_t>(intensity_q8) / LV_SCALE_NONE, INT16_MIN, INT16_MAX);
}

// A strength that starts at its peak and falls to zero over the track.
inline uint8_t fade_out(uint8_t peak, uint32_t intensity_q8, int32_t p) {
  return static_cast<uint8_t>(std::min<int32_t>(scaled(peak, intensity_q8), LV_OPA_COVER) * (ONE - p) / ONE);
}

// Triangle 0 -> 1 -> 0 over the track: the shape of everything that goes out and comes back.
inline int32_t triangle(int32_t p) { return (p < ONE / 2 ? p : ONE - p) * 2; }

// Progress of a track at `time` in the curve domain, or false while it is not running (before delay, after end).
bool progress_of(const Resolved &r, uint32_t time, int32_t &p) {
  if (r.duration == 0 || time < r.delay) return false;
  const uint32_t local = time - r.delay;
  if (local >= r.duration) return false;
  p = curve_step(r.curve, static_cast<int32_t>(local * ONE / r.duration));  // local < 2^16: no overflow
  p = std::clamp<int32_t>(p, 0, ONE);
  return true;
}

void contribute_track(const Resolved &r, uint32_t time, uint32_t intensity_q8, Offsets &out) {
  int32_t p;
  if (!progress_of(r, time, p)) return;
  Offsets o;
  switch (r.kind) {
    case TrackKind::PULSE: {
      const int32_t delta = scaled(r.scale_delta_q8, intensity_q8) * triangle(p) / ONE;
      o.scale = static_cast<uint16_t>(std::clamp<int32_t>(LV_SCALE_NONE + delta, 0, UINT16_MAX));
      break;
    }
    case TrackKind::NUDGE:
      o.translate_x = static_cast<int16_t>(scaled(r.x, intensity_q8) * triangle(p) / ONE);
      o.translate_y = static_cast<int16_t>(scaled(r.y, intensity_q8) * triangle(p) / ONE);
      break;
    case TrackKind::SHAKE: {
      // `cycles` sine periods over the track; the sine is zero at both ends.  Degrees wrap before the table lookup.
      const int32_t angle = static_cast<int32_t>(static_cast<uint32_t>(p) * r.cycles * 360 / ONE) % 360;
      const int32_t sine = lv_trigo_sin(static_cast<int16_t>(angle));  // +-2^15
      const int16_t offset = static_cast<int16_t>((scaled(r.x, intensity_q8) * sine) >> LV_TRIGO_SHIFT);
      if (r.axis == Axis::Y)
        o.translate_y = offset;
      else
        o.translate_x = offset;
      break;
    }
    case TrackKind::FLASH:
      // Highlight at full strength, fading back to the base color; no onset ramp: a flash is a hit.
      o.color = r.color;
      o.color_amount = r.profile == FlashProfile::TRIANGLE
          ? static_cast<uint8_t>(std::min<int32_t>(scaled(r.amount, intensity_q8), LV_OPA_COVER) * triangle(p) / ONE)
          : fade_out(r.amount, intensity_q8, p);
      break;
    case TrackKind::IMAGE_PULSE:
      o.image_opa_add = static_cast<uint8_t>(r.amount * (ONE - p) / ONE);
      break;
    case TrackKind::GLOW:
      o.outline_opa = fade_out(r.amount, intensity_q8, p);
      break;
    case TrackKind::BLINK: {
      const int32_t drop = std::min<int32_t>(scaled(LV_OPA_COVER - r.amount, intensity_q8), LV_OPA_COVER);
      o.opa = static_cast<uint8_t>(LV_OPA_COVER - drop * triangle(p) / ONE);
      break;
    }
    case TrackKind::COUNT:
      return;
  }
  out.merge(o);
}

uint8_t kind_props(TrackKind kind) {
  switch (kind) {
    case TrackKind::PULSE:
      return PROP_SCALE;
    case TrackKind::NUDGE:
    case TrackKind::SHAKE:
      return PROP_TRANSLATE;
    case TrackKind::FLASH:
      return PROP_COLOR;
    case TrackKind::IMAGE_PULSE:
      return PROP_IMAGE_OPA;
    case TrackKind::GLOW:
      return PROP_OUTLINE;
    case TrackKind::BLINK:
      return PROP_OPA;
    case TrackKind::COUNT:
      break;
  }
  return 0;
}

}  // namespace

uint32_t effect_length(const EffectDef &def) {
  uint32_t length = 0;
  for (uint8_t i = 0; i < def.track_count; i++) {
    const Resolved r = resolve_track(def, def.tracks[i]);
    length = std::max(length, r.delay + r.duration);
  }
  if (length > UINT16_MAX) {
    static bool warned = false;
    if (!warned) {
      warned = true;
      ESP_LOGW(TAG, "Effect longer than 65535 ms: cut off at 65535 ms");
    }
  }
  return std::min<uint32_t>(length, UINT16_MAX);
}

uint8_t effect_props(const EffectDef &def) {
  uint8_t props = 0;
  for (uint8_t i = 0; i < def.track_count; i++) props |= kind_props(def.tracks[i].kind);
  return props;
}

uint8_t effect_glow_peak(const EffectDef &def) {
  uint8_t peak = 0;
  for (uint8_t i = 0; i < def.track_count; i++) {
    if (def.tracks[i].kind == TrackKind::GLOW) peak = std::max(peak, resolve_track(def, def.tracks[i]).amount);
  }
  return peak;
}

FlashProp effect_flash_property(const EffectDef &def) {
  for (uint8_t i = 0; i < def.track_count; i++) {
    if (def.tracks[i].kind == TrackKind::FLASH) return resolve_track(def, def.tracks[i]).property;
  }
  return FlashProp::AUTO;
}

void effect_contribute(const EffectDef &def, uint32_t time_ms, uint32_t intensity_q8, Offsets &out) {
  for (uint8_t i = 0; i < def.track_count; i++)
    contribute_track(resolve_track(def, def.tracks[i]), time_ms, intensity_q8, out);
}

}  // namespace lvgl_fx
}  // namespace esphome
