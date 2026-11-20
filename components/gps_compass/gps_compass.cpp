#include "gps_compass.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gps_compass {

static const char *const TAG = "gps_compass";

const char *source_name(Source source) {
  switch (source) {
    case Source::GPS:
      return "gps";
    case Source::MAGNETIC:
      return "magnetic";
    case Source::LAST_GPS:
      return "last_gps";
    default:
      return "none";
  }
}

const char *status_name(Status status) {
  switch (status) {
    case Status::WAITING_FOR_FIX:
      return "waiting_for_fix";
    case Status::WAITING_FOR_MOTION:
      return "waiting_for_motion";
    case Status::CHECK_RECEPTION:
      return "check_reception";
    case Status::WAITING_FOR_MAGNETOMETER:
      return "waiting_for_magnetometer";
    case Status::COLLECTING:
      return "collecting";
    case Status::SAVED_FIT:
      return "saved_fit";
    case Status::CALIBRATED:
      return "calibrated";
  }
  return "unknown";
}

static const char *const AXIS_NAMES[] = {"X", "Y", "Z"};

void GpsCompass::dump_config() {
  ESP_LOGCONFIG(TAG,
                "GPS compass:\n"
                "  Forward axis: %c%s, right axis: %c%s\n"
                "  Level: pitch %.2f, roll %.2f deg\n"
                "  Learning speed >= %.1f km/h, moving >= %.1f km/h, stopped < %.1f km/h\n"
                "  Max HDOP: %.1f, min satellites: %u\n"
                "  Field filter: %.2f, min field: %.1f, sensor timeout: %" PRIu32 " ms, GPS timeout: %" PRIu32 " ms\n"
                "  Observation every %" PRIu32 " ms, fit every %" PRIu32 " ms\n"
                "  Fit needs %u sectors, max gap %u, %u new samples\n"
                "  Magnetometer from sensors: %s, accelerometer from sensors: %s\n"
                "  Saved fit: %s (%.0f samples)",
                this->forward_sign_ < 0 ? '-' : '+', AXIS_NAMES[this->forward_axis_], this->right_sign_ < 0 ? '-' : '+',
                AXIS_NAMES[this->right_axis_], this->pitch_offset_, this->roll_offset_, this->min_learning_speed_,
                this->moving_speed_, this->stopped_speed_, this->max_hdop_, this->min_satellites_, this->field_filter_,
                this->min_field_strength_, this->sensor_timeout_ms_, this->gps_timeout_ms_,
                this->observation_interval_ms_, this->fit_interval_ms_, this->rules_.min_sectors,
                this->rules_.max_sector_gap, this->rules_.min_new_samples, YESNO(this->mag_src_[0] != nullptr),
                YESNO(this->accel_src_[0] != nullptr), YESNO(this->is_calibrated()), this->calibration_.samples);
  LOG_SENSOR("  ", "Heading", this->heading_sensor_);
  LOG_SENSOR("  ", "Magnetic heading", this->magnetic_heading_sensor_);
  LOG_SENSOR("  ", "GPS heading", this->gps_heading_sensor_);
  LOG_SENSOR("  ", "Pitch", this->pitch_sensor_);
  LOG_SENSOR("  ", "Roll", this->roll_sensor_);
  LOG_SENSOR("  ", "Samples", this->samples_sensor_);
  LOG_TEXT_SENSOR("  ", "Source", this->source_sensor_);
  LOG_TEXT_SENSOR("  ", "Status", this->status_sensor_);
  LOG_BINARY_SENSOR("  ", "Calibrated", this->calibrated_sensor_);
}

void GpsCompass::setup() {
  // TinyGPSCustom keeps pointers into the receiver, so these live on the heap and never move.  The talker prefixes
  // are the ones TinyGPSPlus supports; a custom field is used so an empty course is not read as 0.
  static const char *const names[kTalkers] = {"GNRMC", "GPRMC", "GARMC", "GBRMC", "GLRMC"};
  auto &receiver = this->get_tiny_gps();
  for (size_t i = 0; i < kTalkers; ++i)
    this->rmc_[i] = std::make_unique<RmcFields>(receiver, names[i]);

  for (size_t axis = 0; axis < 3; ++axis) {
    if (this->mag_src_[axis] != nullptr) {
      this->mag_src_[axis]->add_on_state_callback([this, axis](float value) {
        this->mag_in_[axis] = value;
        this->mag_mask_ |= static_cast<uint8_t>(1u << axis);
        if (this->mag_mask_ == 7) {
          this->mag_mask_ = 0;
          this->update_field(this->mag_in_[0], this->mag_in_[1], this->mag_in_[2]);
        }
      });
    }
    if (this->accel_src_[axis] != nullptr) {
      this->accel_src_[axis]->add_on_state_callback([this, axis](float value) {
        this->accel_in_[axis] = value;
        this->accel_mask_ |= static_cast<uint8_t>(1u << axis);
        if (this->accel_mask_ == 7) {
          this->accel_mask_ = 0;
          this->update_accel(this->accel_in_[0], this->accel_in_[1], this->accel_in_[2]);
        }
      });
    }
  }
  this->load_state_();
  this->learn_offsets_ = {this->pitch_offset_, this->roll_offset_};
}

void GpsCompass::loop() {
  const uint32_t now = millis();
  if (now - this->last_pose_ms_ >= kPoseIntervalMs) {
    this->last_pose_ms_ = now;
    this->update_pose_(now);
  }
  if (now - this->last_process_ms_ >= kProcessIntervalMs) {
    this->last_process_ms_ = now;
    this->process_(now);
    this->publish_(now);
  }
}

// ---- Inputs -----------------------------------------------------------------------------------------------------

void GpsCompass::update_field(float x, float y, float z) {
  this->field_.update({{x, y, z}}, millis(), this->field_filter_, this->min_field_strength_, this->sensor_timeout_ms_);
}

void GpsCompass::update_accel(float x, float y, float z) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
    this->accel_at_ = 0;
    return;
  }
  this->accel_ = {{x, y, z}};
  this->accel_at_ = millis();
  if (this->accel_at_ == 0)
    this->accel_at_ = 1;
}

void GpsCompass::on_update(TinyGPSPlus &) {
  // Called after every complete NMEA sentence.  An RMC is consumed once: it is the only source of the speed and the
  // course (km/h = knots * 1.852).
  for (auto &rmc : this->rmc_) {
    if (!rmc || !rmc->course.isUpdated())
      continue;
    this->rmc_valid_ = std::strcmp(rmc->status.value(), "A") == 0;
    this->speed_kmh_ = math::parse_number(rmc->speed.value()) * 1.852f;
    this->course_ = math::parse_number(rmc->course.value());
    this->rmc_at_ = millis();
    this->rmc_new_ = true;
  }
}

// ---- Pose -------------------------------------------------------------------------------------------------------

void GpsCompass::update_pose_(uint32_t now) {
  if (!this->accel_at_ || now - this->accel_at_ >= kAccelBlankMs) {
    this->pitch_avg_.clear();
    this->roll_avg_.clear();
    this->pitch_raw_ = this->roll_raw_ = NAN;
  } else {
    // Nose-up and right-side-down angles of the configured axes.  Smoothed before the level is subtracted, so a
    // level change takes effect at once instead of dragging through the window.
    this->pitch_raw_ = this->pitch_avg_.push(math::tilt_degrees(this->accel_, this->forward_axis_, this->forward_sign_));
    this->roll_raw_ = this->roll_avg_.push(math::tilt_degrees(this->accel_, this->right_axis_, -this->right_sign_));
  }
  this->apply_level_();
}

void GpsCompass::apply_level_() {
  this->pitch_ = this->pitch_raw_ - this->pitch_offset_;
  this->roll_ = this->roll_raw_ - this->roll_offset_;
}

bool GpsCompass::level_to_current() {
  if (!std::isfinite(this->pitch_raw_) || !std::isfinite(this->roll_raw_)) {
    ESP_LOGW(TAG, "Cannot level: no accelerometer data");
    return false;
  }
  this->pitch_offset_ = this->pitch_raw_;
  this->roll_offset_ = this->roll_raw_;
  this->level_changed_();
  return true;
}

void GpsCompass::set_level_offsets(optional<float> pitch, optional<float> roll) {
  if (pitch.has_value() && std::isfinite(*pitch))
    this->pitch_offset_ = *pitch;
  if (roll.has_value() && std::isfinite(*roll))
    this->roll_offset_ = *roll;
  this->level_changed_();
}

void GpsCompass::level_changed_() {
  this->level_set_ = true;
  this->apply_level_();
  ESP_LOGI(TAG, "Level: pitch %+.2f, roll %+.2f deg%s", this->pitch_offset_, this->roll_offset_,
           this->is_calibrated() ? "" : " (the saved fit does not match; learning again)");
  this->save_state_();
}

void GpsCompass::reset_calibration() {
  this->calibration_ = {};
  this->grid_.clear();
  this->stats_ = {};
  this->fitted_count_ = 0;
  this->done_ = false;
  this->save_state_();
  ESP_LOGI(TAG, "Calibration reset");
}

// ---- Processing (every 250 ms) ------------------------------------------------------------------------------------

void GpsCompass::process_(uint32_t now) {
  auto &gps = this->get_tiny_gps();
  const uint32_t timeout = this->gps_timeout_ms_;

  this->reverse_active_ = this->reverse_.has_value() && this->reverse_.value();
  this->fixed_ = this->rmc_valid_ && now - this->rmc_at_ <= timeout && gps.location.isValid() &&
                 gps.location.age() <= timeout && gps.satellites.isValid() && gps.satellites.age() <= timeout &&
                 gps.satellites.value() >= this->min_satellites_;
  this->hdop_ok_ = gps.hdop.isValid() && gps.hdop.age() <= timeout && gps.hdop.hdop() > 0.0f &&
                   gps.hdop.hdop() <= this->max_hdop_;
  this->moving_ = this->fixed_ && this->speed_kmh_ >= this->moving_speed_ && this->course_ >= 0.0f &&
                  this->course_ < 360.0f;
  this->stopped_ = this->fixed_ && this->speed_kmh_ >= 0.0f && this->speed_kmh_ < this->stopped_speed_;

  // Live GPS heading while moving (reversing: the vehicle points 180 degrees away from its course).
  this->gps_heading_ = this->moving_ ? std::fmod(this->course_ + (this->reverse_active_ ? 180.0f : 0.0f), 360.0f) : NAN;
  if (this->moving_)
    this->last_heading_ = this->gps_heading_;
  if (!this->fixed_)
    this->last_heading_ = NAN;
  this->stopped_heading_ = this->stopped_ ? this->last_heading_ : NAN;

  // Magnetic heading from the calibrated fit.
  this->field_fresh_ = this->field_.fresh(now, this->sensor_timeout_ms_);
  const bool accel_fresh = this->accel_at_ != 0 && now - this->accel_at_ <= this->sensor_timeout_ms_;
  this->pose_valid_ = accel_fresh && math::compute_pose(this->pitch_, this->roll_, this->pose_);
  this->magnetic_heading_ = NAN;
  if (this->field_fresh_ && this->pose_valid_ && this->is_calibrated())
    this->magnetic_heading_ = math::magnetic_heading(this->calibration_, this->field_.field, this->pose_);

  const auto choice = math::select_heading(this->gps_heading_, this->magnetic_heading_, this->stopped_heading_);
  this->heading_ = choice.heading;
  this->source_ = choice.source;

  // Learning: a level change invalidates what was collected (it was made at the old level).
  const std::array<float, 2> offsets{this->pitch_offset_, this->roll_offset_};
  if (offsets != this->learn_offsets_) {
    this->grid_.clear();
    this->fitted_count_ = 0;
    this->learn_offsets_ = offsets;
    this->done_ = false;
  }
  this->stats_ = this->grid_.stats();
  this->status_ = math::learning_status(this->done_, this->is_calibrated(), this->fixed_, this->moving_,
                                        this->speed_kmh_, this->min_learning_speed_, this->hdop_ok_,
                                        this->field_fresh_);
  const bool rmc_new = this->rmc_new_ && now - this->rmc_at_ <= kRmcMaxAgeMs;
  this->rmc_new_ = false;
  this->learn_(now, rmc_new, this->course_);
}

void GpsCompass::learn_(uint32_t now, bool rmc_new, float course) {
  if (!rmc_new || now - this->last_sample_ < this->observation_interval_ms_ || !this->moving_ || !this->hdop_ok_ ||
      this->reverse_active_ || !(this->speed_kmh_ >= this->min_learning_speed_) || !this->field_fresh_ ||
      now - this->field_.at > kLearnFieldAgeMs || !this->pose_valid_)
    return;
  this->last_sample_ = now;
  // The raw (unfiltered) field and the course of this very RMC form one observation.
  this->grid_.add(course, this->pose_, this->field_.raw);
  this->stats_ = this->grid_.stats();
  if (now - this->last_fit_ < this->fit_interval_ms_)
    return;
  this->last_fit_ = now;

  const bool has_saved_fit = this->is_calibrated();
  math::Calibration fit;
  const auto result = math::try_fit(this->grid_, this->rules_, this->fitted_count_,
                                    has_saved_fit ? this->calibration_.samples : 0.0f, this->pitch_offset_,
                                    this->roll_offset_, fit);
  if (result != math::FitResult::ACCEPTED) {
    ESP_LOGV(TAG, "Fit not accepted (%d): %u samples in %u sectors", static_cast<int>(result), this->stats_.samples,
             this->stats_.sectors);
    return;
  }
  this->calibration_ = fit;
  this->fitted_count_ = static_cast<unsigned>(fit.samples);
  const bool first = !this->done_;
  this->done_ = true;
  this->status_ = Status::CALIBRATED;
  this->save_state_();
  ESP_LOGI(TAG, "Calibration %s: %u samples in %u sectors", first ? "saved" : "improved", this->stats_.samples,
           this->stats_.sectors);
  this->calibrated_callbacks_.call(first, this->stats_.samples);
}

// ---- Outputs ----------------------------------------------------------------------------------------------------

static int bearing_of(float heading) {
  return std::isfinite(heading) ? (static_cast<int>(std::lround(heading)) + 360) % 360 : -1;
}

static void publish_bearing(sensor::Sensor *sensor, float heading, int &last) {
  const int bearing = bearing_of(heading);
  if (sensor == nullptr || bearing == last)
    return;
  last = bearing;
  sensor->publish_state(heading);
}

static void publish_angle(sensor::Sensor *sensor, float value, float &last, bool &valid) {
  if (sensor == nullptr)
    return;
  const bool finite = std::isfinite(value);
  if (finite == valid && (!finite || std::abs(value - last) < 0.1f))
    return;
  valid = finite;
  if (finite)
    last = value;
  sensor->publish_state(value);
}

void GpsCompass::publish_(uint32_t) {
  publish_bearing(this->heading_sensor_, this->heading_, this->last_heading_pub_);
  publish_bearing(this->magnetic_heading_sensor_, this->magnetic_heading_, this->last_magnetic_pub_);
  publish_bearing(this->gps_heading_sensor_, this->gps_heading_, this->last_gps_pub_);
  publish_angle(this->pitch_sensor_, this->pitch_, this->last_pitch_pub_, this->pitch_pub_valid_);
  publish_angle(this->roll_sensor_, this->roll_, this->last_roll_pub_, this->roll_pub_valid_);
  if (this->samples_sensor_ != nullptr && static_cast<int>(this->stats_.samples) != this->last_samples_pub_) {
    this->last_samples_pub_ = static_cast<int>(this->stats_.samples);
    this->samples_sensor_->publish_state(this->stats_.samples);
  }
  if (this->source_sensor_ != nullptr && static_cast<int>(this->source_) != this->last_source_pub_) {
    this->last_source_pub_ = static_cast<int>(this->source_);
    this->source_sensor_->publish_state(gps_compass::source_name(this->source_));
  }
  if (this->status_sensor_ != nullptr && static_cast<int>(this->status_) != this->last_status_pub_) {
    this->last_status_pub_ = static_cast<int>(this->status_);
    this->status_sensor_->publish_state(gps_compass::status_name(this->status_));
  }
  if (this->calibrated_sensor_ != nullptr)
    this->calibrated_sensor_->publish_state(this->is_calibrated());
}

// ---- Persistence ------------------------------------------------------------------------------------------------

static bool all_finite(const StoredState &s) {
  if (!std::isfinite(s.pitch_offset) || !std::isfinite(s.roll_offset))
    return false;
  for (const auto &row : s.fit)
    for (float v : row)
      if (!std::isfinite(v))
        return false;
  return true;
}

void GpsCompass::load_state_() {
  char key[64];
  std::snprintf(key, sizeof(key), "gps_compass_v%" PRIu32 "_%s", kStoreVersion, this->key_);
  this->pref_ = global_preferences->make_preference<StoredState>(fnv1_hash(key), true);
  StoredState s{};
  if (!this->pref_.load(&s) || s.version != kStoreVersion || !all_finite(s)) {
    ESP_LOGD(TAG, "No stored calibration");
    return;
  }
  if (s.level_set) {
    this->pitch_offset_ = s.pitch_offset;
    this->roll_offset_ = s.roll_offset;
    this->level_set_ = true;
  }
  math::Calibration &c = this->calibration_;
  c.center = {{s.fit[0][0], s.fit[0][1], s.fit[0][2]}};
  c.north = {{s.fit[1][0], s.fit[1][1], s.fit[1][2]}};
  c.east = {{s.fit[2][0], s.fit[2][1], s.fit[2][2]}};
  c.gravity_forward = {{s.fit[3][0], s.fit[3][1], s.fit[3][2]}};
  c.gravity_right = {{s.fit[4][0], s.fit[4][1], s.fit[4][2]}};
  c.pitch_offset = s.fit[5][0];
  c.roll_offset = s.fit[5][1];
  c.samples = s.fit[5][2];
  ESP_LOGI(TAG, "Restored level pitch %+.2f roll %+.2f; fit of %.0f samples %s", this->pitch_offset_,
           this->roll_offset_, c.samples, this->is_calibrated() ? "in use" : "not valid for this level");
}

void GpsCompass::save_state_() {
  StoredState s{};
  s.version = kStoreVersion;
  s.level_set = this->level_set_ ? 1 : 0;
  s.pitch_offset = this->pitch_offset_;
  s.roll_offset = this->roll_offset_;
  const math::Calibration &c = this->calibration_;
  const math::Vec3 *rows[5] = {&c.center, &c.north, &c.east, &c.gravity_forward, &c.gravity_right};
  for (size_t i = 0; i < 5; ++i)
    for (size_t axis = 0; axis < 3; ++axis)
      s.fit[i][axis] = (*rows[i])[axis];
  s.fit[5][0] = c.pitch_offset;
  s.fit[5][1] = c.roll_offset;
  s.fit[5][2] = c.samples;
  if (!this->pref_.save(&s))
    ESP_LOGW(TAG, "Saving the calibration failed");
}

}  // namespace gps_compass
}  // namespace esphome
