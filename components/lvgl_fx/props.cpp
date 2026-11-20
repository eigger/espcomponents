#include "props.h"
#include "lvgl_fx.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome {
namespace lvgl_fx {

static const char *const TAG = "lvgl_fx";

// x * y / 255 with exact identity for 255 (the rounding term is < 255/2).
static inline uint8_t mul_opa(uint8_t a, uint8_t b) { return static_cast<uint8_t>((a * b + 127) / 255); }

static inline int16_t add_sat(int16_t a, int16_t b) {
  return static_cast<int16_t>(std::clamp<int32_t>(static_cast<int32_t>(a) + b, INT16_MIN, INT16_MAX));
}

void Offsets::merge(const Offsets &other) {
  this->opa = mul_opa(this->opa, other.opa);
  // Q8 multiply; exact identity for 256.
  this->scale = static_cast<uint16_t>(
      std::min<uint32_t>((static_cast<uint32_t>(this->scale) * other.scale + 128) >> 8, UINT16_MAX));
  this->rotation += other.rotation;
  this->translate_x = add_sat(this->translate_x, other.translate_x);
  this->translate_y = add_sat(this->translate_y, other.translate_y);
  this->outline_opa = std::max(this->outline_opa, other.outline_opa);
  this->image_opa_add = std::max(this->image_opa_add, other.image_opa_add);
  if (other.color_amount == 0) return;
  if (this->color_amount == 0) {
    this->color = other.color;
    this->color_amount = other.color_amount;
  } else {
    this->color = lv_color_mix(other.color, this->color, other.color_amount);
    this->color_amount = std::max(this->color_amount, other.color_amount);
  }
}

// ---- contributors ------------------------------------------------------------------------------------------------

// Displacement 0..255 of a visible style: 0 at the opacity the target has while shown, 255 at opacity 0.  Derived from
// the base opacity the visible transition animates, so offset and opacity share one clock and a reversal continues
// from wherever the opacity is.
static uint32_t visible_displacement(const Slot &slot, const Instance &instance) {
  const uint32_t shown = instance.state;
  if (shown == 0) return 0;
  const uint32_t reached = std::min<uint32_t>((slot.base_opa * 255u + shown / 2) / shown, 255u);
  return 255u - reached;
}

static void contribute_one(const Slot &slot, const Instance &instance, Offsets &out) {
  Offsets offset;
  switch (instance.kind) {
    case InstanceKind::NONE:
    case InstanceKind::COUNT:
    case InstanceKind::TYPEWRITER:
    case InstanceKind::DECODE:
    case InstanceKind::LINE_RUN:
    case InstanceKind::LINE_REST:
      return;
    case InstanceKind::EFFECT:
      // Tracks merge into `out` themselves (several tracks, one pass).
      effect_contribute(*static_cast<const EffectDef *>(instance.def), instance.progress,
                        detail::core().intensity_q8, out);
      return;
    case InstanceKind::TEXT_ENTER: {
      const int32_t p = std::min<int32_t>(instance.progress, LV_OPA_COVER);
      const int32_t from = instance.arg.b;
      offset.opa = static_cast<uint8_t>(from + ((LV_OPA_COVER - from) * p + 127) / LV_OPA_COVER);
      offset.translate_y = static_cast<int16_t>(instance.arg.a * (LV_OPA_COVER - p) / LV_OPA_COVER);
      break;
    }
    case InstanceKind::SLIDE_UP:
    case InstanceKind::SLIDE_DOWN:
    case InstanceKind::SLIDE_LEFT:
    case InstanceKind::SLIDE_RIGHT: {
      // The entry offset points away from the travel direction: slide_up starts below and moves up.
      const int16_t px = static_cast<int16_t>((instance.arg.a * visible_displacement(slot, instance) + 127) / 255);
      switch (instance.kind) {
        case InstanceKind::SLIDE_UP:
          offset.translate_y = px;
          break;
        case InstanceKind::SLIDE_DOWN:
          offset.translate_y = static_cast<int16_t>(-px);
          break;
        case InstanceKind::SLIDE_LEFT:
          offset.translate_x = px;
          break;
        default:
          offset.translate_x = static_cast<int16_t>(-px);
          break;
      }
      break;
    }
    case InstanceKind::ZOOM: {
      // arg.a = starting scale (Q8): 256 at rest, `from` at full displacement.
      const int32_t displacement = static_cast<int32_t>(visible_displacement(slot, instance));
      const int32_t delta = (LV_SCALE_NONE - instance.arg.a) * displacement;
      offset.scale = static_cast<uint16_t>(LV_SCALE_NONE - (delta + (delta >= 0 ? 127 : -127)) / 255);
      break;
    }
  }
  out.merge(offset);
}

void contribute(const Slot &slot, Offsets &out) {
  contribute_one(slot, slot.one_shot, out);
  contribute_one(slot, slot.loop, out);
  contribute_one(slot, slot.style, out);
  contribute_one(slot, slot.vis, out);
}

uint8_t instance_props(const Instance &instance) {
  switch (instance.kind) {
    case InstanceKind::EFFECT:
      return instance.state;
    case InstanceKind::TEXT_ENTER:
      return instance.arg.a != 0 ? (PROP_OPA | PROP_TRANSLATE) : PROP_OPA;
    case InstanceKind::SLIDE_UP:
    case InstanceKind::SLIDE_DOWN:
    case InstanceKind::SLIDE_LEFT:
    case InstanceKind::SLIDE_RIGHT:
      return PROP_TRANSLATE;
    case InstanceKind::ZOOM:
      return PROP_SCALE;
    case InstanceKind::NONE:
    case InstanceKind::COUNT:
    case InstanceKind::TYPEWRITER:
    case InstanceKind::DECODE:
    case InstanceKind::LINE_RUN:
    case InstanceKind::LINE_REST:
      break;
  }
  return 0;
}

// ---- color access ------------------------------------------------------------------------------------------------

ColorTarget color_target_of(lv_obj_t *obj) {
#if LV_USE_LABEL
  if (lv_obj_check_type(obj, &lv_label_class)) return ColorTarget::TEXT;
#endif
#if LV_USE_IMAGE
  if (lv_obj_check_type(obj, &lv_image_class)) return ColorTarget::IMAGE;
#endif
  return ColorTarget::BG;
}

lv_color_t read_color(lv_obj_t *obj, ColorTarget target) {
  switch (target) {
    case ColorTarget::TEXT:
      return lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    case ColorTarget::IMAGE:
      return lv_obj_get_style_image_recolor(obj, LV_PART_MAIN);
    case ColorTarget::BG:
      break;
  }
  return lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
}

void write_color(lv_obj_t *obj, ColorTarget target, lv_color_t color) {
  switch (target) {
    case ColorTarget::TEXT:
      lv_obj_set_style_text_color(obj, color, LV_PART_MAIN);
      return;
    case ColorTarget::IMAGE:
      // Recolor has no effect at opa 0 (the LVGL default): tint fully so the color is what the caller asked for.
      if (lv_obj_get_style_image_recolor_opa(obj, LV_PART_MAIN) == LV_OPA_TRANSP)
        lv_obj_set_style_image_recolor_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_image_recolor(obj, color, LV_PART_MAIN);
      return;
    case ColorTarget::BG:
      break;
  }
  lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
}

// ---- ownership and composition -----------------------------------------------------------------------------------

static void take_color(Slot &slot, ColorTarget target) {
  slot.color_target = target;
  slot.base_color = slot.color_from = slot.color_to = read_color(slot.obj, target);
  // An image whose recolor is off shows no tint: a contribution to its color has to supply the tint itself (compose).
  if (target == ColorTarget::IMAGE && lv_obj_get_style_image_recolor_opa(slot.obj, LV_PART_MAIN) == LV_OPA_TRANSP)
    slot.flags |= SLOT_UNTINTED;
  else
    slot.flags &= static_cast<uint8_t>(~SLOT_UNTINTED);
}

bool own_color(Slot &slot, ColorTarget target, bool busy) {
  if (slot.props & PROP_COLOR) {
    if (slot.color_target == target) return true;
    if (busy) return false;
    // Leave the old property as it was: its base value, without any tint of ours.
    if (slot.flags & SLOT_UNTINTED)
      lv_obj_set_style_image_recolor_opa(slot.obj, LV_OPA_TRANSP, LV_PART_MAIN);
    else
      write_color(slot.obj, slot.color_target, slot.base_color);
  }
  take_color(slot, target);
  slot.props |= PROP_COLOR;
  return true;
}

// The outline opacity the widget has at rest.  A focused or pressed widget would yield the theme's value for that
// state, so outside LV_STATE_DEFAULT only the widget's own default-state style counts.
static uint8_t default_state_outline_opa(lv_obj_t *obj) {
  if (lv_obj_get_state(obj) == LV_STATE_DEFAULT) return lv_obj_get_style_outline_opa(obj, LV_PART_MAIN);
  lv_style_value_t value;
  const lv_style_selector_t selector = static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT;
  if (lv_obj_get_local_style_prop(obj, LV_STYLE_OUTLINE_OPA, &value, selector) == LV_STYLE_RES_FOUND)
    return static_cast<uint8_t>(value.num);
  return static_cast<uint8_t>(lv_style_prop_get_default(LV_STYLE_OUTLINE_OPA).num);
}

void own(Slot &slot, uint8_t props) {
  props &= static_cast<uint8_t>(~slot.props);
  if (props == 0) return;
  lv_obj_t *obj = slot.obj;

  if (props & PROP_OPA) {
    slot.base_opa = slot.visible_opa = lv_obj_get_style_opa(obj, LV_PART_MAIN);
  }
  if (props & PROP_SCALE) {
    const int32_t scale = lv_obj_get_style_transform_scale_x(obj, LV_PART_MAIN);
    slot.base_scale = static_cast<uint16_t>(std::clamp<int32_t>(scale, 0, 0xFFFF));
    // LVGL scales around the top-left corner unless a pivot is set: grow from the middle instead.  Only for a target
    // that is not transformed already (no scale, no rotation, no pivot): anything else was set up on purpose.
    if (scale == LV_SCALE_NONE && lv_obj_get_style_transform_rotation(obj, LV_PART_MAIN) == 0 &&
        lv_obj_get_style_transform_pivot_x(obj, LV_PART_MAIN) == 0 &&
        lv_obj_get_style_transform_pivot_y(obj, LV_PART_MAIN) == 0) {
      lv_obj_set_style_transform_pivot_x(obj, lv_pct(50), LV_PART_MAIN);
      lv_obj_set_style_transform_pivot_y(obj, lv_pct(50), LV_PART_MAIN);
    }
  }
  if (props & PROP_ROTATION) {
    const int32_t rotation = lv_obj_get_style_transform_rotation(obj, LV_PART_MAIN);
    slot.base_rotation = static_cast<int16_t>(std::clamp<int32_t>(rotation, INT16_MIN, INT16_MAX));
  }
  if (props & PROP_COLOR) take_color(slot, color_target_of(obj));
  if (props & PROP_IMAGE_OPA) slot.base_image_opa = lv_obj_get_style_image_opa(obj, LV_PART_MAIN);
  if (props & PROP_OUTLINE) {
    slot.base_outline_opa = default_state_outline_opa(obj);
    // Glow only moves the outline's opacity: without a width there is nothing to see.
    static bool warned = false;
    if (!warned && lv_obj_get_style_outline_width(obj, LV_PART_MAIN) == 0) {
      warned = true;
      ESP_LOGW(TAG, "An outline effect needs outline_width and outline_color set on the target");
    }
  }
  // PROP_TRANSLATE has a base of 0: nothing to read.

  slot.props |= props;

  // Scale and rotation make LVGL render the target through an intermediate layer; plain opacity does not.
  // Warn once if the target covers a quarter of the screen or more.
  if ((props & (PROP_SCALE | PROP_ROTATION)) && !(slot.flags & SLOT_LARGE_WARNED)) {
    lv_display_t *disp = lv_obj_get_display(obj);
    if (disp != nullptr) {
      const int64_t area = static_cast<int64_t>(lv_obj_get_width(obj)) * lv_obj_get_height(obj);
      const int64_t screen = static_cast<int64_t>(lv_display_get_horizontal_resolution(disp)) *
                             lv_display_get_vertical_resolution(disp);
      if (screen > 0 && area * 4 >= screen) {
        slot.flags |= SLOT_LARGE_WARNED;
        ESP_LOGW(TAG, "Scale/rotation on a target covering >=25%% of the screen is rendered through a layer "
                      "and may drop frames; prefer translate/opacity/color motion for large areas");
      }
    }
  }
}

void compose(Slot &slot, uint8_t mask) {
  mask &= slot.props;
  if (mask == 0) return;
  lv_obj_t *obj = slot.obj;

  Offsets off;
  contribute(slot, off);

  if (mask & PROP_OPA) lv_obj_set_style_opa(obj, mul_opa(slot.base_opa, off.opa), LV_PART_MAIN);
  if (mask & PROP_IMAGE_OPA)
    lv_obj_set_style_image_opa(obj, std::min<int>(slot.base_image_opa + off.image_opa_add, LV_OPA_COVER), LV_PART_MAIN);
  if (mask & PROP_SCALE) {
    const uint32_t scale = (static_cast<uint32_t>(slot.base_scale) * off.scale + 128) >> 8;
    lv_obj_set_style_transform_scale_x(obj, scale, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(obj, scale, LV_PART_MAIN);
  }
  if (mask & PROP_ROTATION)
    lv_obj_set_style_transform_rotation(obj, static_cast<int32_t>(slot.base_rotation) + off.rotation, LV_PART_MAIN);
  if (mask & PROP_COLOR) {
    if (slot.flags & SLOT_UNTINTED) {
      // No base tint: the contribution is the tint, fading in and out through the recolor opacity.
      if (off.color_amount != 0) lv_obj_set_style_image_recolor(obj, off.color, LV_PART_MAIN);
      lv_obj_set_style_image_recolor_opa(obj, off.color_amount, LV_PART_MAIN);
    } else {
      lv_color_t color = slot.base_color;
      if (off.color_amount != 0) color = lv_color_mix(off.color, color, off.color_amount);
      write_color(obj, slot.color_target, color);
    }
  }
  if (mask & PROP_OUTLINE)
    lv_obj_set_style_outline_opa(obj, std::max(slot.base_outline_opa, off.outline_opa), LV_PART_MAIN);
  if (mask & PROP_TRANSLATE) {
    lv_obj_set_style_translate_x(obj, off.translate_x, LV_PART_MAIN);
    lv_obj_set_style_translate_y(obj, off.translate_y, LV_PART_MAIN);
  }
}

}  // namespace lvgl_fx
}  // namespace esphome
