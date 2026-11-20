#pragma once

// Pure math for gps_compass.  No ESPHome includes: compiled by the host tests in tests/native/gps_compass_math.
//
// Model (per magnetometer axis):  field ~ c0 + a*(forward*N) + b*(right*E - forward_right*N) + g1*down_forward
//                                          + g2*down_right
// where N/E are the GPS course's north/east unit components, `forward`/`right` the horizontal vehicle axes expressed
// in the tilted sensor frame, and (down_forward, down_right) the gravity direction in the vehicle frame.  The fit
// learns c0 (hard iron), a/b (the horizontal field, per sensor axis) and g1/g2 (how pitch/roll change the reading);
// the heading is then the inverse (dual-basis) projection of a measured field.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace esphome {
namespace gps_compass {
namespace math {

using Vec3 = std::array<float, 3>;
constexpr size_t kTerms = 5;
using Features = std::array<float, kTerms>;

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

// Constants that stay fixed (the speeds, timings and counts are options of the component).
constexpr float kMinDownVerticalSq = 0.25f;  // gravity must be >60 deg from the vehicle plane normal
constexpr float kRidge = 0.006f;             // regularisation of the two gravity terms
constexpr float kSingularPivot = 0.00001f;
constexpr float kMinHorizontalSq = 64.0f;    // |a|^2, |b|^2 >= 64 (8 uT horizontal field per sensor)
constexpr float kMinAxisSeparation = 0.25f;  // det >= 0.25 * |a|^2 * |b|^2 (a, b not collinear)
constexpr float kResidualFactor = 0.12f;     // err^2 <= 0.12 * min(|a|^2, |b|^2)
constexpr float kSlopeDeadband = 0.035f;     // |down_forward|, |down_right| below this are "level"
constexpr float kMinStrength = 0.4f;         // accepted |(N,E)| of a measured heading
constexpr float kMaxStrength = 1.6f;
constexpr size_t kSectors = 8;               // 45 deg course sectors
constexpr size_t kSlopes = 5;                // level, nose down, nose up, right down, left down

inline float dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

inline float parse_number(const char *text) {
  if (text == nullptr)
    return NAN;
  char *end;
  const float value = std::strtof(text, &end);
  return end != text && *end == '\0' && std::isfinite(value) ? value : NAN;
}

inline float normalize_degrees(float degrees) {
  if (!std::isfinite(degrees))
    return NAN;
  float wrapped = std::fmod(degrees, 360.0f);
  if (wrapped < 0.0f)
    wrapped += 360.0f;
  return wrapped >= 360.0f ? 0.0f : wrapped;
}

// ---- Tilt ------------------------------------------------------------------------------------------------------

// Angle of `axis` out of the plane of the other two, from an accelerometer vector (reaction to gravity, so "up").
// atan2(sign * a[axis], hypot(other two)); the reference device uses exactly this for pitch and roll.
inline float tilt_degrees(const Vec3 &accel, size_t axis, float sign) {
  const float a = accel[(axis + 1) % 3];
  const float b = accel[(axis + 2) % 3];
  return std::atan2(sign * accel[axis], std::sqrt(a * a + b * b)) * kRadToDeg;
}

// Moving average over the last N finite samples; NaN input empties it (stale sensor -> unknown, not a stale mean).
template<size_t N> class MovingAverage {
 public:
  float push(float value) {
    if (!std::isfinite(value)) {
      this->clear();
      return NAN;
    }
    this->values_[this->next_] = value;
    this->next_ = (this->next_ + 1) % N;
    this->count_ = std::min(this->count_ + 1, N);
    return this->mean();
  }
  float mean() const {
    if (this->count_ == 0)
      return NAN;
    float sum = 0.0f;
    for (size_t i = 0; i < this->count_; ++i)
      sum += this->values_[i];
    return sum / static_cast<float>(this->count_);
  }
  void clear() { this->count_ = this->next_ = 0; }

 protected:
  std::array<float, N> values_{};
  size_t next_{0}, count_{0};
};

// ---- Pose ------------------------------------------------------------------------------------------------------

struct Pose {
  float down_forward{}, down_right{};
  float forward{};        // horizontal forward axis ...
  float forward_right{};  // ... and right axis expressed along the sensor's forward/right axes
  float right{};
};

// pitch: nose-up positive, roll: right-side-down positive (both leveled, degrees).  False when the mounting is tilted
// so far (>~60 deg) that the horizontal axes are ill-defined, or when an angle is not finite.
inline bool compute_pose(float pitch_deg, float roll_deg, Pose &pose) {
  const float down_forward = -std::sin(pitch_deg * kDegToRad);
  const float down_right = std::sin(roll_deg * kDegToRad);
  const float down_vertical_sq = 1.0f - down_forward * down_forward - down_right * down_right;
  if (!(down_vertical_sq > kMinDownVerticalSq))
    return false;
  const float forward = std::sqrt(1.0f - down_forward * down_forward);
  pose = {down_forward, down_right, forward, -down_forward * down_right / forward,
          std::sqrt(down_vertical_sq) / forward};
  return true;
}

// ---- Calibration -----------------------------------------------------------------------------------------------

struct Calibration {
  Vec3 center{};            // hard-iron / field offset
  Vec3 north{}, east{};     // dual basis: project (field - center - gravity terms) to get forward*N, right*E-fr*N
  Vec3 gravity_forward{}, gravity_right{};
  float pitch_offset{0.0f}, roll_offset{0.0f};  // level the fit was made with
  float samples{0.0f};                          // observation count of the fit; 0 = no fit

  bool matches_level(float pitch, float roll) const { return pitch_offset == pitch && roll_offset == roll; }
  bool usable_for(float pitch, float roll) const {
    return samples > 0.0f && dot(this->north, this->north) > 0.0f && this->matches_level(pitch, roll);
  }
};

// Dual basis of the horizontal field vectors a (north) and b (east).  False when too small or too collinear.
inline bool dual_basis(const Vec3 &a, const Vec3 &b, Vec3 &north_projection, Vec3 &east_projection) {
  const float nn = dot(a, a);
  const float ee = dot(b, b);
  const float ne = dot(a, b);
  const float det = nn * ee - ne * ne;
  if (std::min(nn, ee) < kMinHorizontalSq || det < kMinAxisSeparation * nn * ee)
    return false;
  for (size_t axis = 0; axis < a.size(); ++axis) {
    north_projection[axis] = (ee * a[axis] - ne * b[axis]) / det;
    east_projection[axis] = (nn * b[axis] - ne * a[axis]) / det;
  }
  return true;
}

// Heading in [0, 360) from a measured field, or NAN (no fit for this level, tilt out of range, strength off).
inline float magnetic_heading(const Calibration &cal, const Vec3 &field, const Pose &pose) {
  Vec3 centered;
  for (size_t axis = 0; axis < centered.size(); ++axis)
    centered[axis] = (field[axis] - cal.center[axis]) -
                     (cal.gravity_forward[axis] * pose.down_forward + cal.gravity_right[axis] * pose.down_right);
  const float north = dot(centered, cal.north) / pose.forward;
  const float east = (dot(centered, cal.east) + pose.forward_right * north) / pose.right;
  const float strength = std::hypot(north, east);
  if (!(strength >= kMinStrength && strength <= kMaxStrength))
    return NAN;
  return normalize_degrees(std::atan2(east, north) * kRadToDeg);
}

// ---- Observations ----------------------------------------------------------------------------------------------

inline Features observation_features(const Pose &pose, float course_deg) {
  const float north = std::cos(course_deg * kDegToRad);
  const float east = std::sin(course_deg * kDegToRad);
  return {1.0f, pose.forward * north, pose.right * east - pose.forward_right * north, pose.down_forward,
          pose.down_right};
}

inline size_t slope_class(const Pose &pose) {
  const float dominant = std::max(std::abs(pose.down_forward), std::abs(pose.down_right));
  if (dominant < kSlopeDeadband)
    return 0;
  return std::abs(pose.down_forward) >= std::abs(pose.down_right) ? (pose.down_forward > 0 ? 1 : 2)
                                                                   : (pose.down_right > 0 ? 3 : 4);
}

inline size_t course_sector(float course_deg) {
  return static_cast<size_t>((course_deg + 22.5f) / 45.0f) % kSectors;
}

struct Observation {
  Features features{};
  Vec3 field{};
  bool present{false};
};

struct GridStats {
  unsigned samples{0};
  unsigned sectors{0};
  size_t max_gap{0};
};

// One representative observation per (course sector, slope class): a single road or banking direction cannot
// dominate the fit.
class ObservationGrid {
 public:
  void clear() { this->cells_ = {}; }
  void add(float course_deg, const Pose &pose, const Vec3 &raw_field) {
    this->cells_[course_sector(course_deg)][slope_class(pose)] = {observation_features(pose, course_deg), raw_field,
                                                                   true};
  }
  const std::array<std::array<Observation, kSlopes>, kSectors> &cells() const { return this->cells_; }

  GridStats stats() const {
    GridStats s;
    size_t first = kSectors, previous = 0;
    for (size_t i = 0; i < kSectors; ++i) {
      const unsigned used = static_cast<unsigned>(
          std::count_if(this->cells_[i].begin(), this->cells_[i].end(), [](const Observation &o) { return o.present; }));
      if (!used)
        continue;
      s.samples += used;
      ++s.sectors;
      if (first == kSectors)
        first = i;
      else
        s.max_gap = std::max(s.max_gap, i - previous);
      previous = i;
    }
    s.max_gap = std::max(s.max_gap, first + kSectors - previous);
    return s;
  }

 protected:
  std::array<std::array<Observation, kSlopes>, kSectors> cells_{};
};

struct FitRules {
  unsigned min_sectors{5};
  unsigned max_sector_gap{3};
  unsigned min_new_samples{4};
};

enum class FitResult : uint8_t {
  ACCEPTED,
  FEW_SECTORS,       // fewer course sectors than min_sectors
  SECTOR_GAP,        // the covered sectors leave too wide a gap
  FEW_NEW_SAMPLES,   // not enough new samples since the last fit
  FEWER_THAN_SAVED,  // would replace a saved fit made from more samples
  SINGULAR,          // normal equations not solvable
  DEGENERATE_AXES,   // horizontal field vectors too small or collinear
  LARGE_RESIDUAL,    // some observation does not fit the model
};

// Least squares over [1, forward*N, right*E - fr*N, down_forward, down_right] for the three sensor axes, with a small
// ridge on the gravity terms (a drive without slope variation must not blow them up), then the acceptance rules.
// `fitted_count`: sample count of the last fit of this session (0 = none).  `saved_samples`: sample count of a saved
// fit made at the same level (0 = none).  `out` is written only when ACCEPTED.
inline FitResult try_fit(const ObservationGrid &grid, const FitRules &rules, unsigned fitted_count, float saved_samples,
                         float pitch_offset, float roll_offset, Calibration &out) {
  const GridStats stats = grid.stats();
  if (stats.sectors < rules.min_sectors)
    return FitResult::FEW_SECTORS;
  if (stats.max_gap > rules.max_sector_gap)
    return FitResult::SECTOR_GAP;
  if (fitted_count && stats.samples < fitted_count + rules.min_new_samples)
    return FitResult::FEW_NEW_SAMPLES;
  if (saved_samples > 0.0f && static_cast<float>(stats.samples) < saved_samples)
    return FitResult::FEWER_THAN_SAVED;

  std::array<std::array<float, kTerms + 3>, kTerms> system{};
  system[3][3] = system[4][4] = kRidge;
  for (const auto &row : grid.cells()) {
    for (const auto &sample : row) {
      if (!sample.present)
        continue;
      for (size_t i = 0; i < kTerms; ++i) {
        for (size_t j = 0; j < kTerms; ++j)
          system[i][j] += sample.features[i] * sample.features[j];
        for (size_t axis = 0; axis < 3; ++axis)
          system[i][kTerms + axis] += sample.features[i] * sample.field[axis];
      }
    }
  }
  for (size_t i = 0; i < kTerms; ++i) {
    size_t pivot = i;
    for (size_t j = i + 1; j < kTerms; ++j)
      if (std::abs(system[j][i]) > std::abs(system[pivot][i]))
        pivot = j;
    if (std::abs(system[pivot][i]) < kSingularPivot)
      return FitResult::SINGULAR;
    std::swap(system[pivot], system[i]);
    const float scale = system[i][i];
    for (auto &value : system[i])
      value /= scale;
    for (size_t j = 0; j < kTerms; ++j) {
      if (j == i)
        continue;
      const float factor = system[j][i];
      for (size_t k = i; k < system[j].size(); ++k)
        system[j][k] -= factor * system[i][k];
    }
  }
  std::array<Vec3, kTerms> fit;
  for (size_t i = 0; i < kTerms; ++i)
    std::copy_n(system[i].begin() + kTerms, 3, fit[i].begin());

  Vec3 north_projection, east_projection;
  if (!dual_basis(fit[1], fit[2], north_projection, east_projection))
    return FitResult::DEGENERATE_AXES;
  const float max_error_sq = kResidualFactor * std::min(dot(fit[1], fit[1]), dot(fit[2], fit[2]));
  for (const auto &row : grid.cells()) {
    for (const auto &sample : row) {
      if (!sample.present)
        continue;
      Vec3 error = sample.field;
      for (size_t i = 0; i < kTerms; ++i)
        for (size_t axis = 0; axis < error.size(); ++axis)
          error[axis] -= fit[i][axis] * sample.features[i];
      if (dot(error, error) > max_error_sq)
        return FitResult::LARGE_RESIDUAL;
    }
  }
  out.center = fit[0];
  out.north = north_projection;
  out.east = east_projection;
  out.gravity_forward = fit[3];
  out.gravity_right = fit[4];
  out.pitch_offset = pitch_offset;
  out.roll_offset = roll_offset;
  out.samples = static_cast<float>(stats.samples);
  return FitResult::ACCEPTED;
}

// ---- Field filter ----------------------------------------------------------------------------------------------

// Smoothed field for the heading, raw field for the fit (smoothing the fit input would bias the hard-iron estimate).
struct FieldFilter {
  Vec3 field{}, raw{};
  bool ready{false};
  uint32_t at{0};

  // Returns false (and drops `ready`) for a non-finite or too weak sample (sensor not producing data yet).
  bool update(const Vec3 &sample, uint32_t now, float alpha, float min_strength, uint32_t reseed_gap_ms) {
    const float magnitude = dot(sample, sample);
    if (!std::isfinite(magnitude) || magnitude < min_strength * min_strength) {
      this->ready = false;
      return false;
    }
    this->raw = sample;
    if (!this->ready || now - this->at > reseed_gap_ms) {
      this->field = sample;
    } else {
      for (size_t axis = 0; axis < this->field.size(); ++axis)
        this->field[axis] += alpha * (sample[axis] - this->field[axis]);
    }
    this->ready = true;
    this->at = now;
    return true;
  }
  bool fresh(uint32_t now, uint32_t timeout_ms) const { return this->ready && now - this->at <= timeout_ms; }
};

// ---- Source and status policy ----------------------------------------------------------------------------------

enum class Source : uint8_t { NONE, GPS, MAGNETIC, LAST_GPS };

struct HeadingChoice {
  float heading{NAN};
  Source source{Source::NONE};
};

// Live GPS course while moving, else the calibrated magnetic heading, else the heading from just before stopping.
inline HeadingChoice select_heading(float live_gps, float magnetic, float stopped_gps) {
  if (std::isfinite(live_gps))
    return {live_gps, Source::GPS};
  if (std::isfinite(magnetic))
    return {magnetic, Source::MAGNETIC};
  if (std::isfinite(stopped_gps))
    return {stopped_gps, Source::LAST_GPS};
  return {};
}

enum class Status : uint8_t {
  WAITING_FOR_FIX,          // no usable GPS fix
  WAITING_FOR_MOTION,       // fix, but not moving / below the learning speed
  CHECK_RECEPTION,          // moving fast enough, HDOP too high or unknown
  WAITING_FOR_MAGNETOMETER, // no fresh magnetometer sample
  COLLECTING,               // gathering course sectors
  SAVED_FIT,                // a saved fit is in use; learning continues and replaces it with a better one
  CALIBRATED,               // a fit was accepted since boot / level change
};

inline Status learning_status(bool done, bool has_saved_fit, bool fixed, bool moving, float speed_kmh,
                              float min_learning_speed, bool hdop_ok, bool field_fresh) {
  if (done)
    return Status::CALIBRATED;
  if (has_saved_fit)
    return Status::SAVED_FIT;
  if (!fixed)
    return Status::WAITING_FOR_FIX;
  if (!moving || !(speed_kmh >= min_learning_speed))
    return Status::WAITING_FOR_MOTION;
  if (!hdop_ok)
    return Status::CHECK_RECEPTION;
  if (!field_fresh)
    return Status::WAITING_FOR_MAGNETOMETER;
  return Status::COLLECTING;
}

}  // namespace math
}  // namespace gps_compass
}  // namespace esphome
