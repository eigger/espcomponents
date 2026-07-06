#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace m5unit_scales {

enum M5UnitScalesModel {
  M5UNIT_SCALES_MODEL_MINI = 0,
  M5UNIT_SCALES_MODEL_STANDARD = 1,
};

class M5UnitScalesComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void loop() override;

  // Configuration setters
  void set_model(M5UnitScalesModel model) { model_ = model; }

  // Setters for platform items
  void set_weight_sensor(sensor::Sensor *weight_sensor) { weight_sensor_ = weight_sensor; }
  void set_raw_adc_sensor(sensor::Sensor *raw_adc_sensor) { raw_adc_sensor_ = raw_adc_sensor; }
  void set_absolute_weight_sensor(sensor::Sensor *absolute_weight_sensor) { absolute_weight_sensor_ = absolute_weight_sensor; }
  void set_button_sensor(binary_sensor::BinarySensor *button_sensor) { button_sensor_ = button_sensor; }
  
  // Custom platform wrappers
  void set_tare_button(button::Button *tare_button) { tare_button_ = tare_button; }
  void set_lp_filter_switch(switch_::Switch *lp_filter_switch) { lp_filter_switch_ = lp_filter_switch; }
  void set_avg_filter_number(number::Number *avg_filter_number) { avg_filter_number_ = avg_filter_number; }
  void set_ema_filter_number(number::Number *ema_filter_number) { ema_filter_number_ = ema_filter_number; }
  void set_gap_number(number::Number *gap_number) { gap_number_ = gap_number; }
  void set_led_light(light::LightOutput *led_light) { led_light_ = led_light; }

  // API methods called by wrapper entities
  void tare();
  void set_lp_filter(bool enable);
  void set_avg_filter(uint8_t avg);
  void set_ema_filter(uint8_t ema);
  void set_gap_value(float gap);
  void set_led_color(uint8_t r, uint8_t g, uint8_t b);

 protected:
  M5UnitScalesModel model_{M5UNIT_SCALES_MODEL_MINI};

  sensor::Sensor *weight_sensor_{nullptr};
  sensor::Sensor *raw_adc_sensor_{nullptr};
  sensor::Sensor *absolute_weight_sensor_{nullptr};

  float tare_offset_{0.0f};
  float last_read_weight_{0.0f};
  binary_sensor::BinarySensor *button_sensor_{nullptr};
  button::Button *tare_button_{nullptr};
  switch_::Switch *lp_filter_switch_{nullptr};
  number::Number *avg_filter_number_{nullptr};
  number::Number *ema_filter_number_{nullptr};
  number::Number *gap_number_{nullptr};
  light::LightOutput *led_light_{nullptr};

  uint32_t last_button_poll_{0};
  bool initial_sync_done_{false};
};

// C++ Platform Wrappers

class M5UnitScalesTareButton : public button::Button {
 public:
  void set_parent(M5UnitScalesComponent *parent) { parent_ = parent; }
  void press_action() override { this->parent_->tare(); }
 protected:
  M5UnitScalesComponent *parent_;
};

class M5UnitScalesLPFilterSwitch : public switch_::Switch {
 public:
  void set_parent(M5UnitScalesComponent *parent) { parent_ = parent; }
  void write_state(bool state) override {
    this->parent_->set_lp_filter(state);
    this->publish_state(state);
  }
 protected:
  M5UnitScalesComponent *parent_;
};

class M5UnitScalesAvgFilterNumber : public number::Number {
 public:
  void set_parent(M5UnitScalesComponent *parent) { parent_ = parent; }
  void control(float value) override {
    this->parent_->set_avg_filter(static_cast<uint8_t>(value));
    this->publish_state(value);
  }
 protected:
  M5UnitScalesComponent *parent_;
};

class M5UnitScalesEmaFilterNumber : public number::Number {
 public:
  void set_parent(M5UnitScalesComponent *parent) { parent_ = parent; }
  void control(float value) override {
    this->parent_->set_ema_filter(static_cast<uint8_t>(value));
    this->publish_state(value);
  }
 protected:
  M5UnitScalesComponent *parent_;
};

class M5UnitScalesGapNumber : public number::Number {
 public:
  void set_parent(M5UnitScalesComponent *parent) { parent_ = parent; }
  void control(float value) override {
    this->parent_->set_gap_value(value);
    this->publish_state(value);
  }
 protected:
  M5UnitScalesComponent *parent_;
};

class M5UnitScalesLED : public light::LightOutput {
 public:
  void set_parent(M5UnitScalesComponent *parent) { parent_ = parent; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void write_state(light::LightState *state) override {
    float r, g, b;
    state->current_values_as_rgb(&r, &g, &b);
    uint8_t red = r * 255;
    uint8_t green = g * 255;
    uint8_t blue = b * 255;
    this->parent_->set_led_color(red, green, blue);
  }
 protected:
  M5UnitScalesComponent *parent_;
};

}  // namespace m5unit_scales
}  // namespace esphome
