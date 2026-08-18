#ifndef __BMM150_H__
#define __BMM150_H__

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "bmm150_lib.h"

namespace esphome {
namespace bmm150 {

struct BMM150Calibration {
  uint8_t version;
  uint8_t mag_src[3];
  int8_t mag_sign[3];
  float mag_ut_scale;
  int16_t offset_x;
  int16_t offset_y;
  int16_t offset_z;
  float scale_x;
  float scale_y;
  float scale_z;
  uint8_t valid;
} PACKED;

struct BMM150AxisMap {
  uint8_t src[3]{0, 1, 2};
  int8_t sign[3]{1, 1, 1};
};

class BMM150Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_mag_x(sensor::Sensor *mag_x) { mag_x_ = mag_x; }
  void set_mag_y(sensor::Sensor *mag_y) { mag_y_ = mag_y; }
  void set_mag_z(sensor::Sensor *mag_z) { mag_z_ = mag_z; }
  void set_heading(sensor::Sensor *heading) { heading_ = heading; }

  void set_accel_x(sensor::Sensor *accel_x) { accel_x_ = accel_x; }
  void set_accel_y(sensor::Sensor *accel_y) { accel_y_ = accel_y; }
  void set_accel_z(sensor::Sensor *accel_z) { accel_z_ = accel_z; }

  void set_declination(float declination) { declination_ = declination; }
  void set_soft_iron(bool soft_iron) { soft_iron_ = soft_iron; }
  void set_mag_axes(uint8_t x_src, int8_t x_sign, uint8_t y_src, int8_t y_sign, uint8_t z_src, int8_t z_sign);
  void set_accel_axes(uint8_t x_src, int8_t x_sign, uint8_t y_src, int8_t y_sign, uint8_t z_src, int8_t z_sign);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_bus_error() { this->bus_error_ = true; }
  void start_calibration(uint32_t duration_ms);
  Trigger<bool> *get_calibration_finished_trigger() { return &this->calibration_finished_trigger_; }

 protected:
  sensor::Sensor *mag_x_{nullptr};
  sensor::Sensor *mag_y_{nullptr};
  sensor::Sensor *mag_z_{nullptr};
  sensor::Sensor *heading_{nullptr};
  sensor::Sensor *accel_x_{nullptr};
  sensor::Sensor *accel_y_{nullptr};
  sensor::Sensor *accel_z_{nullptr};

  struct bmm150_dev dev_;
  struct bmm150_mag_data mag_data_;
  bool bus_error_{false};
  bool initialized_{false};

  float declination_{0.0f};
  bool soft_iron_{true};
  BMM150AxisMap mag_axes_;
  BMM150AxisMap accel_axes_;

  BMM150Calibration calib_{};
  ESPPreferenceObject pref_;
  bool calibrating_{false};
  float cal_min_[3]{};
  float cal_max_[3]{};
  bool tilt_unavailable_logged_{false};
  Trigger<bool> calibration_finished_trigger_;

  int8_t bmm150_initialization();
  void load_calibration_();
  void save_calibration_();
  void finish_calibration_();
  void reset_calibration_();
  void stamp_calibration_context_(BMM150Calibration *out) const;
  bool calibration_matches_config_(const BMM150Calibration &c) const;
  void apply_axes_(const float in[3], const BMM150AxisMap &map, float out[3]) const;
  bool has_accel_ids_() const;
  bool read_accel_(float accel[3]) const;
  float compute_planar_heading_(float mx, float my) const;
  float compute_tilt_heading_(float mx, float my, float mz, const float accel[3]) const;
  static float wrap_degrees_(float deg);
};

template<typename... Ts> class CalibrateAction : public Action<Ts...> {
 public:
  explicit CalibrateAction(BMM150Component *parent) : parent_(parent) {}
  void set_duration(uint32_t duration_ms) { this->duration_ms_ = duration_ms; }
  void play(const Ts &...x) override { this->parent_->start_calibration(this->duration_ms_); }

 protected:
  BMM150Component *parent_;
  uint32_t duration_ms_{30000};
};

}  // namespace bmm150
}  // namespace esphome
#endif
