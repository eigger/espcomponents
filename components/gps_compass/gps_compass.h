#pragma once

#include <memory>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/gps/gps.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"

#include "compass_math.h"

namespace esphome {
namespace gps_compass {

using math::Source;
using math::Status;

const char *source_name(Source source);
const char *status_name(Status status);

// Persisted state: the saved fit (with the level it was made at) and the pitch/roll zero.  Fixed layout; bump
// kStoreVersion when it changes (also part of the preference key, so an old blob is never decoded as a new struct).
struct StoredState {
  uint32_t version;
  uint32_t level_set;  // 1 once the level was set or cleared at runtime (then it overrides the YAML defaults)
  float pitch_offset;
  float roll_offset;
  float fit[6][3];  // center, north, east, gravity_forward, gravity_right, {pitch_offset, roll_offset, samples}
};

class GpsCompass : public Component, public gps::GPSListener {
 public:
  // ---- Configuration (codegen) ----
  void set_preference_key(const char *key) { this->key_ = key; }
  void set_magnetometer(sensor::Sensor *x, sensor::Sensor *y, sensor::Sensor *z) { this->mag_src_ = {x, y, z}; }
  void set_accelerometer(sensor::Sensor *x, sensor::Sensor *y, sensor::Sensor *z) { this->accel_src_ = {x, y, z}; }
  void set_forward_axis(uint8_t axis, float sign) { this->forward_axis_ = axis, this->forward_sign_ = sign; }
  void set_right_axis(uint8_t axis, float sign) { this->right_axis_ = axis, this->right_sign_ = sign; }
  void set_initial_level(float pitch, float roll) { this->pitch_offset_ = pitch, this->roll_offset_ = roll; }
  void set_reverse(TemplatableFn<bool> reverse) { this->reverse_ = reverse; }
  void set_min_learning_speed(float v) { this->min_learning_speed_ = v; }
  void set_moving_speed(float v) { this->moving_speed_ = v; }
  void set_stopped_speed(float v) { this->stopped_speed_ = v; }
  void set_max_hdop(float v) { this->max_hdop_ = v; }
  void set_min_satellites(uint8_t v) { this->min_satellites_ = v; }
  void set_field_filter(float v) { this->field_filter_ = v; }
  void set_min_field_strength(float v) { this->min_field_strength_ = v; }
  void set_sensor_timeout(uint32_t ms) { this->sensor_timeout_ms_ = ms; }
  void set_gps_timeout(uint32_t ms) { this->gps_timeout_ms_ = ms; }
  void set_observation_interval(uint32_t ms) { this->observation_interval_ms_ = ms; }
  void set_fit_interval(uint32_t ms) { this->fit_interval_ms_ = ms; }
  void set_min_sectors(uint8_t v) { this->rules_.min_sectors = v; }
  void set_max_sector_gap(uint8_t v) { this->rules_.max_sector_gap = v; }
  void set_min_new_samples(uint8_t v) { this->rules_.min_new_samples = v; }
  void set_heading_sensor(sensor::Sensor *s) { this->heading_sensor_ = s; }
  void set_magnetic_heading_sensor(sensor::Sensor *s) { this->magnetic_heading_sensor_ = s; }
  void set_gps_heading_sensor(sensor::Sensor *s) { this->gps_heading_sensor_ = s; }
  void set_pitch_sensor(sensor::Sensor *s) { this->pitch_sensor_ = s; }
  void set_roll_sensor(sensor::Sensor *s) { this->roll_sensor_ = s; }
  void set_samples_sensor(sensor::Sensor *s) { this->samples_sensor_ = s; }
  void set_source_text_sensor(text_sensor::TextSensor *s) { this->source_sensor_ = s; }
  void set_status_text_sensor(text_sensor::TextSensor *s) { this->status_sensor_ = s; }
  void set_calibrated_binary_sensor(binary_sensor::BinarySensor *s) { this->calibrated_sensor_ = s; }
  void add_on_calibrated_callback(std::function<void(bool, uint32_t)> &&cb) {
    this->calibrated_callbacks_.add(std::move(cb));
  }

  // ---- Component ----
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // ---- GPS ----
  void on_update(TinyGPSPlus &gps) override;

  // ---- Inputs (for sources that only expose a callback; the sensor ids of the config call these) ----
  /// Magnetometer sample, any consistent unit (the defaults assume microtesla), any axis order.
  void update_field(float x, float y, float z);
  /// Accelerometer sample (reaction to gravity: +1 g along "up"), any unit; the axes must be the ones named by
  /// forward_axis / right_axis.
  void update_accel(float x, float y, float z);

  // ---- Level (pitch / roll zero) ----
  /// Take the current (smoothed) pose as level.  False when there is no fresh accelerometer data.
  bool level_to_current();
  /// Set the raw pitch / roll readings (degrees, nose-up / right-down positive) that count as level; omitted = keep.
  void set_level_offsets(optional<float> pitch, optional<float> roll);
  void clear_level() { this->set_level_offsets(0.0f, 0.0f); }
  float pitch_offset() const { return this->pitch_offset_; }
  float roll_offset() const { return this->roll_offset_; }

  /// Forget the fit, the collected samples and the saved copy.  Level is kept.
  void reset_calibration();

  // ---- Heading ----
  /// Selected heading in [0, 360), true north; NAN when there is none.
  float heading() const { return this->heading_; }
  Source source() const { return this->source_; }
  const char *source_name() const { return gps_compass::source_name(this->source_); }
  /// Calibrated magnetometer heading regardless of the selected source (NAN when not available).
  float magnetic_heading() const { return this->magnetic_heading_; }
  /// GPS course (+180 when reverse) while moving, else NAN.
  float gps_heading() const { return this->gps_heading_; }
  /// Heading from just before the vehicle stopped (while stopped), else NAN.
  float stopped_heading() const { return this->stopped_heading_; }

  // ---- State (everything a status line or dial needs) ----
  bool has_fix() const { return this->fixed_; }
  bool is_moving() const { return this->moving_; }
  bool is_stopped() const { return this->stopped_; }
  bool hdop_ok() const { return this->hdop_ok_; }
  bool is_reverse() const { return this->reverse_active_; }
  float speed_kmh() const { return this->speed_kmh_; }
  bool field_fresh() const { return this->field_fresh_; }
  bool pose_valid() const { return this->pose_valid_; }
  /// A fit made at the current level exists (saved or learned this session).
  bool is_calibrated() const { return this->calibration_.usable_for(this->pitch_offset_, this->roll_offset_); }
  /// A fit was accepted since boot / level change / reset.
  bool learning_done() const { return this->done_; }
  /// Observation cells collected since boot / level change / reset, and the course sectors they cover.
  uint32_t sample_count() const { return this->stats_.samples; }
  uint8_t sector_count() const { return this->stats_.sectors; }
  /// Sample count of the fit currently in use (0 = none).
  uint32_t fit_sample_count() const { return is_calibrated() ? static_cast<uint32_t>(this->calibration_.samples) : 0; }
  Status status() const { return this->status_; }
  const char *status_name() const { return gps_compass::status_name(this->status_); }
  /// Leveled, smoothed pitch (nose up +) and roll (right side down +) in degrees; NAN without accelerometer data.
  float pitch() const { return this->pitch_; }
  float roll() const { return this->roll_; }

 protected:
  struct RmcFields {
    TinyGPSCustom status, speed, course;
    RmcFields(TinyGPSPlus &receiver, const char *name)
        : status(receiver, name, 2), speed(receiver, name, 7), course(receiver, name, 8) {}
  };
  static constexpr size_t kTalkers = 5;
  static constexpr uint32_t kPoseIntervalMs = 200;      // pitch / roll sampling (5-sample moving average)
  static constexpr uint32_t kProcessIntervalMs = 250;   // GPS gates, learning, outputs
  static constexpr uint32_t kRmcMaxAgeMs = 750;         // an observation needs an RMC this young
  static constexpr uint32_t kLearnFieldAgeMs = 500;     // ... and a magnetometer sample this young
  static constexpr uint32_t kAccelBlankMs = 3000;       // pitch / roll go unknown after this
  static constexpr uint32_t kStoreVersion = 1;

  void update_pose_(uint32_t now);
  void process_(uint32_t now);
  void learn_(uint32_t now, bool rmc_new, float course);
  void publish_(uint32_t now);
  void apply_level_();
  void level_changed_();
  void load_state_();
  void save_state_();

  // Inputs
  std::array<sensor::Sensor *, 3> mag_src_{}, accel_src_{};
  math::Vec3 mag_in_{}, accel_in_{}, accel_{};
  uint8_t mag_mask_{0}, accel_mask_{0};
  math::FieldFilter field_{};
  uint32_t accel_at_{0};
  std::array<std::unique_ptr<RmcFields>, kTalkers> rmc_{};
  bool rmc_valid_{false}, rmc_new_{false};
  float speed_kmh_{NAN}, course_{NAN};
  uint32_t rmc_at_{0};

  // Configuration
  const char *key_{"gps_compass"};
  uint8_t forward_axis_{1}, right_axis_{0};
  float forward_sign_{1.0f}, right_sign_{1.0f};
  TemplatableFn<bool> reverse_{};
  float min_learning_speed_{18.0f}, moving_speed_{8.0f}, stopped_speed_{3.0f};
  float max_hdop_{3.0f};
  uint8_t min_satellites_{4};
  float field_filter_{0.35f}, min_field_strength_{2.0f};
  uint32_t sensor_timeout_ms_{1500}, gps_timeout_ms_{3000}, observation_interval_ms_{900}, fit_interval_ms_{3000};
  math::FitRules rules_{};

  // Level and pose
  float pitch_offset_{0.0f}, roll_offset_{0.0f};
  bool level_set_{false};
  math::MovingAverage<5> pitch_avg_, roll_avg_;
  float pitch_raw_{NAN}, roll_raw_{NAN}, pitch_{NAN}, roll_{NAN};
  math::Pose pose_{};
  bool pose_valid_{false};

  // GPS-derived state
  bool fixed_{false}, hdop_ok_{false}, moving_{false}, stopped_{false}, reverse_active_{false}, field_fresh_{false};
  float gps_heading_{NAN}, stopped_heading_{NAN}, last_heading_{NAN}, magnetic_heading_{NAN}, heading_{NAN};
  Source source_{Source::NONE};
  Status status_{Status::WAITING_FOR_FIX};

  // Learning
  math::ObservationGrid grid_{};
  math::GridStats stats_{};
  math::Calibration calibration_{};
  std::array<float, 2> learn_offsets_{};
  unsigned fitted_count_{0};
  bool done_{false};
  uint32_t last_sample_{0}, last_fit_{0}, last_pose_ms_{0}, last_process_ms_{0};
  ESPPreferenceObject pref_{};

  // Outputs
  sensor::Sensor *heading_sensor_{nullptr}, *magnetic_heading_sensor_{nullptr}, *gps_heading_sensor_{nullptr};
  sensor::Sensor *pitch_sensor_{nullptr}, *roll_sensor_{nullptr}, *samples_sensor_{nullptr};
  text_sensor::TextSensor *source_sensor_{nullptr}, *status_sensor_{nullptr};
  binary_sensor::BinarySensor *calibrated_sensor_{nullptr};
  CallbackManager<void(bool, uint32_t)> calibrated_callbacks_{};
  int last_heading_pub_{-2}, last_magnetic_pub_{-2}, last_gps_pub_{-2}, last_samples_pub_{-1};
  float last_pitch_pub_{NAN}, last_roll_pub_{NAN};
  bool pitch_pub_valid_{false}, roll_pub_valid_{false};
  int last_source_pub_{-1}, last_status_pub_{-1};
};

}  // namespace gps_compass
}  // namespace esphome
