#include "props.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome {
namespace lvgl_fx {

static const char *const TAG = "lvgl_fx";

// x * y / 255 with exact identity for 255 (the rounding term is < 255/2).
static inline uint8_t mul_opa(uint8_t a, uint8_t b) { return static_cast<uint8_t>((a * b + 127) / 255); }

void Offsets::merge(const Offsets &other) {
  this->opa = mul_opa(this->opa, other.opa);
  // Q8 multiply; exact identity for 256.
  this->scale = static_cast<uint16_t>((static_cast<uint32_t>(this->scale) * other.scale + 128) >> 8);
  this->rotation += other.rotation;
  this->translate_x += other.translate_x;
  this->translate_y += other.translate_y;
  this->outline_opa = std::max(this->outline_opa, other.outline_opa);
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

static void contribute_one(const Instance &instance, Offsets &out) {
  Offsets offset;
  switch (instance.kind) {
    case InstanceKind::NONE:
      return;
    case InstanceKind::FADE_IN:
      offset.opa = static_cast<uint8_t>(std::min<uint16_t>(instance.progress, LV_OPA_COVER));
      break;
  }
  out.merge(offset);
}

void contribute(const Slot &slot, Offsets &out) {
  contribute_one(slot.one_shot, out);
  contribute_one(slot.loop, out);
  contribute_one(slot.style, out);
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

void own(Slot &slot, uint8_t props) {
  props &= static_cast<uint8_t>(~slot.props);
  if (props == 0) return;
  lv_obj_t *obj = slot.obj;

  if (props & PROP_OPA) {
    slot.base_opa = slot.visible_opa = lv_obj_get_style_opa(obj, LV_PART_MAIN);
  }
  if (props & PROP_SCALE) {
    slot.base_scale = static_cast<uint16_t>(std::clamp<int32_t>(lv_obj_get_style_transform_scale_x(obj, LV_PART_MAIN), 0, 0xFFFF));
  }
  if (props & PROP_ROTATION) {
    slot.base_rotation = static_cast<int16_t>(std::clamp<int32_t>(lv_obj_get_style_transform_rotation(obj, LV_PART_MAIN), INT16_MIN, INT16_MAX));
  }
  if (props & PROP_COLOR) {
    slot.color_target = color_target_of(obj);
    slot.base_color = slot.color_from = slot.color_to = read_color(obj, slot.color_target);
  }
  // PROP_OUTLINE and PROP_TRANSLATE have a base of 0 (6): nothing to read.

  slot.props |= props;

  // Opacity, scale and rotation make LVGL render the target through an intermediate layer (7.5): warn once if the
  // target covers a quarter of the screen or more.
  if ((props & (PROP_OPA | PROP_SCALE | PROP_ROTATION)) && !(slot.flags & SLOT_LARGE_WARNED)) {
    lv_display_t *disp = lv_obj_get_display(obj);
    if (disp != nullptr) {
      const int64_t area = static_cast<int64_t>(lv_obj_get_width(obj)) * lv_obj_get_height(obj);
      const int64_t screen = static_cast<int64_t>(lv_display_get_horizontal_resolution(disp)) *
                             lv_display_get_vertical_resolution(disp);
      if (screen > 0 && area * 4 >= screen) {
        slot.flags |= SLOT_LARGE_WARNED;
        ESP_LOGW(TAG, "Opacity/scale/rotation on a target covering >=25%% of the screen is rendered through a layer "
                      "and may drop frames; prefer translate/color motion for large areas");
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
  if (mask & PROP_SCALE) {
    const uint32_t scale = (static_cast<uint32_t>(slot.base_scale) * off.scale + 128) >> 8;
    lv_obj_set_style_transform_scale_x(obj, scale, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(obj, scale, LV_PART_MAIN);
  }
  if (mask & PROP_ROTATION)
    lv_obj_set_style_transform_rotation(obj, static_cast<int32_t>(slot.base_rotation) + off.rotation, LV_PART_MAIN);
  if (mask & PROP_COLOR) {
    lv_color_t color = slot.base_color;
    if (off.color_amount != 0) color = lv_color_mix(off.color, color, off.color_amount);
    write_color(obj, slot.color_target, color);
  }
  if (mask & PROP_OUTLINE) lv_obj_set_style_outline_opa(obj, off.outline_opa, LV_PART_MAIN);
  if (mask & PROP_TRANSLATE) {
    lv_obj_set_style_translate_x(obj, off.translate_x, LV_PART_MAIN);
    lv_obj_set_style_translate_y(obj, off.translate_y, LV_PART_MAIN);
  }
}

}  // namespace lvgl_fx
}  // namespace esphome
