// Host tests for components/gps_compass/compass_math.h (no ESPHome needed).
#include "compass_math.h"

#include <cstdio>
#include <cstdlib>
#include <random>

using namespace esphome::gps_compass::math;

static int g_failed = 0, g_checks = 0;
#define CHECK(cond)                                                          \
  do {                                                                       \
    ++g_checks;                                                              \
    if (!(cond)) {                                                           \
      ++g_failed;                                                            \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
    }                                                                        \
  } while (0)
#define CHECK_NEAR(a, b, tol)                                                                          \
  do {                                                                                                 \
    ++g_checks;                                                                                        \
    const double va = (a), vb = (b);                                                                   \
    if (!(std::fabs(va - vb) <= (tol))) {                                                              \
      ++g_failed;                                                                                      \
      std::printf("  FAIL %s:%d: %s=%g vs %s=%g (tol %g)\n", __FILE__, __LINE__, #a, va, #b, vb, (double) (tol)); \
    }                                                                                                  \
  } while (0)

// Smallest signed angular difference in degrees.
static float angle_diff(float a, float b) {
  float d = std::fmod(a - b + 540.0f, 360.0f) - 180.0f;
  return d;
}

// ---- Synthetic vehicle ------------------------------------------------------------------------------------------

struct Vehicle {
  Vec3 c0{{14.0f, -22.0f, 31.0f}};  // hard iron
  Vec3 a{{18.0f, 4.0f, -6.0f}};     // field of "forward*N" per sensor axis
  Vec3 b{{-5.0f, 17.0f, 3.0f}};     // field of "right*E - fr*N"
  Vec3 g1{{12.0f, -4.0f, 6.0f}};    // gravity coupling
  Vec3 g2{{-3.0f, 10.0f, 5.0f}};
  float noise{0.3f};
  float second_harmonic{0.0f};  // soft-iron-like distortion that the model cannot represent (fraction of |a|)

  Vec3 field(float course, const Pose &pose, std::mt19937 &rng) const {
    const Features f = observation_features(pose, course);
    std::uniform_real_distribution<float> n(-noise, noise);
    Vec3 out;
    for (size_t i = 0; i < 3; ++i) {
      out[i] = c0[i] + a[i] * f[1] + b[i] * f[2] + g1[i] * f[3] + g2[i] * f[4] + n(rng);
      out[i] += second_harmonic * std::sqrt(dot(a, a)) * std::cos(2.0f * course * kDegToRad) * (i == 0 ? 1.0f : 0.5f);
    }
    return out;
  }
};

struct Tilt {
  float pitch, roll;
};
// One tilt per slope class: level, nose up, nose down, right down, left down.
static const Tilt kTilts[] = {{0, 0}, {6, 0}, {-6, 0}, {0, 5}, {0, -5}};

static ObservationGrid drive(const Vehicle &v, std::mt19937 &rng, float course_step = 5.0f, size_t tilts = 5) {
  ObservationGrid grid;
  for (size_t t = 0; t < tilts; ++t) {
    Pose pose;
    CHECK(compute_pose(kTilts[t].pitch, kTilts[t].roll, pose));
    for (float course = 0.0f; course < 360.0f; course += course_step)
      grid.add(course, pose, v.field(course, pose, rng));
  }
  return grid;
}

// Largest heading error over a grid of courses and tilts (including tilts that were not in the training set).
static float max_heading_error(const Vehicle &v, const Calibration &cal, std::mt19937 &rng) {
  float worst = 0.0f;
  const Tilt tests[] = {{0, 0}, {3, 2}, {-4, 3}, {5, -4}, {0, 6}};
  for (const Tilt &t : tests) {
    Pose pose;
    if (!compute_pose(t.pitch, t.roll, pose))
      return 999.0f;
    for (float course = 0.0f; course < 360.0f; course += 11.0f) {
      const float h = magnetic_heading(cal, v.field(course, pose, rng), pose);
      if (!std::isfinite(h))
        return 998.0f;
      worst = std::max(worst, std::abs(angle_diff(h, course)));
    }
  }
  return worst;
}

// ---- Tests ------------------------------------------------------------------------------------------------------

static void test_pose() {
  Pose p;
  CHECK(compute_pose(0.0f, 0.0f, p));
  CHECK_NEAR(p.forward, 1.0f, 1e-6);
  CHECK_NEAR(p.right, 1.0f, 1e-6);
  CHECK_NEAR(p.forward_right, 0.0f, 1e-6);
  CHECK_NEAR(p.down_forward, 0.0f, 1e-6);

  // Reference expressions evaluated by hand for pitch 10 (nose up), roll 5 (right down).
  CHECK(compute_pose(10.0f, 5.0f, p));
  const float df = -std::sin(10.0f * kDegToRad), dr = std::sin(5.0f * kDegToRad);
  const float vert = std::sqrt(1.0f - df * df - dr * dr), fwd = std::sqrt(1.0f - df * df);
  CHECK_NEAR(p.down_forward, df, 1e-7);
  CHECK_NEAR(p.down_right, dr, 1e-7);
  CHECK_NEAR(p.forward, fwd, 1e-7);
  CHECK_NEAR(p.forward_right, -df * dr / fwd, 1e-7);
  CHECK_NEAR(p.right, vert / fwd, 1e-7);

  // Too-extreme tilt: vertical gravity share <= 0.25 (60+ degrees) and non-finite input are rejected.
  CHECK(compute_pose(50.0f, 0.0f, p));
  CHECK(!compute_pose(65.0f, 0.0f, p));
  CHECK(!compute_pose(0.0f, -65.0f, p));
  CHECK(!compute_pose(45.0f, 45.0f, p));
  CHECK(!compute_pose(NAN, 0.0f, p));
  CHECK(!compute_pose(0.0f, INFINITY, p));
}

static void test_tilt_matches_reference_formulas() {
  // The Tab5 reference: pitch_raw = atan2(-az, hypot(ax, ay)), roll = atan2(-ay, hypot(ax, az)), pitch direction -1
  // applied in the pose.  gps_compass expresses it as forward_axis +Z (nose-up = atan2(+az, ...)), right_axis +Y.
  std::mt19937 rng(7);
  std::uniform_real_distribution<float> d(-1.0f, 1.0f);
  for (int i = 0; i < 200; ++i) {
    const Vec3 acc{{d(rng), d(rng), d(rng)}};
    const float ref_pitch = std::atan2(-acc[2], std::sqrt(acc[0] * acc[0] + acc[1] * acc[1])) * 180.0f / kPi;
    const float ref_roll = std::atan2(-acc[1], std::sqrt(acc[0] * acc[0] + acc[2] * acc[2])) * 180.0f / kPi;
    const float nose_up = tilt_degrees(acc, 2, +1.0f);
    const float right_down = tilt_degrees(acc, 1, -1.0f);
    CHECK_NEAR(nose_up, -ref_pitch, 1e-4);   // = ref_pitch * tab5_pitch_direction(-1)
    CHECK_NEAR(right_down, ref_roll, 1e-4);  // tab5_roll_direction(+1)
  }
  CHECK_NEAR(tilt_degrees({{0.0f, 0.0f, 1.0f}}, 2, 1.0f), 90.0f, 1e-4);
  CHECK_NEAR(tilt_degrees({{0.0f, 0.5f, 0.866025f}}, 2, 1.0f), 60.0f, 1e-3);
  CHECK_NEAR(tilt_degrees({{0.0f, 0.5f, 0.866025f}}, 2, -1.0f), -60.0f, 1e-3);
}

static void test_moving_average() {
  MovingAverage<5> m;
  CHECK(std::isnan(m.mean()));
  CHECK_NEAR(m.push(10.0f), 10.0f, 1e-6);
  CHECK_NEAR(m.push(20.0f), 15.0f, 1e-6);
  for (int i = 0; i < 5; ++i)
    m.push(30.0f);
  CHECK_NEAR(m.mean(), 30.0f, 1e-6);  // 10 and 20 have left the window of 5
  CHECK(std::isnan(m.push(NAN)));
  CHECK_NEAR(m.push(4.0f), 4.0f, 1e-6);  // starts fresh after a gap
}

static void test_grid() {
  Pose level;
  compute_pose(0, 0, level);
  CHECK(course_sector(0.0f) == 0);
  CHECK(course_sector(22.4f) == 0);
  CHECK(course_sector(22.5f) == 1);
  CHECK(course_sector(359.9f) == 0);
  CHECK(course_sector(337.4f) == 7);
  CHECK(slope_class(level) == 0);
  Pose p;
  compute_pose(6, 0, p);   // nose up -> down_forward < 0
  CHECK(slope_class(p) == 2);
  compute_pose(-6, 0, p);
  CHECK(slope_class(p) == 1);
  compute_pose(0, 5, p);
  CHECK(slope_class(p) == 3);
  compute_pose(0, -5, p);
  CHECK(slope_class(p) == 4);
  compute_pose(1.5f, 1.0f, p);  // below the 0.035 deadband
  CHECK(slope_class(p) == 0);

  ObservationGrid grid;
  const Vec3 f{{1, 2, 3}};
  GridStats s = grid.stats();
  CHECK(s.samples == 0 && s.sectors == 0);
  for (float course : {0.0f, 45.0f, 90.0f, 135.0f, 180.0f})
    grid.add(course, level, f);
  s = grid.stats();
  CHECK(s.samples == 5 && s.sectors == 5);
  CHECK(s.max_gap == 4);  // 5..7 and 0 are empty: wrap-around gap 0 + 8 - 4
  grid.add(225.0f, level, f);
  grid.add(270.0f, level, f);
  CHECK(grid.stats().max_gap == 2);
  grid.add(10.0f, level, f);  // same cell again: replaces, not adds
  CHECK(grid.stats().samples == 7);
  compute_pose(6, 0, p);
  grid.add(10.0f, p, f);  // other slope class: new cell
  CHECK(grid.stats().samples == 8);
  grid.clear();
  CHECK(grid.stats().samples == 0);
}

static void test_dual_basis() {
  const Vec3 a{{18.0f, 4.0f, -6.0f}}, b{{-5.0f, 17.0f, 3.0f}};
  Vec3 np, ep;
  CHECK(dual_basis(a, b, np, ep));
  CHECK_NEAR(dot(np, a), 1.0f, 1e-5);
  CHECK_NEAR(dot(np, b), 0.0f, 1e-5);
  CHECK_NEAR(dot(ep, b), 1.0f, 1e-5);
  CHECK_NEAR(dot(ep, a), 0.0f, 1e-5);
  CHECK(!dual_basis({{3, 0, 0}}, {{0, 3, 0}}, np, ep));          // |a|^2 = 9 < 64
  CHECK(!dual_basis(a, {{18.0f, 4.0f, -6.5f}}, np, ep));         // collinear
  CHECK(!dual_basis(a, {{-9.0f, -2.0f, 3.0f}}, np, ep));         // collinear, opposite
  CHECK(dual_basis({{20, 0, 0}}, {{0, 20, 0}}, np, ep));
  // Exactly at the 0.25 separation bound: sin^2(angle) = 0.25 -> angle 30 degrees passes, 29 does not.
  const float c30 = std::cos(31.0f * kDegToRad), s30 = std::sin(31.0f * kDegToRad);
  CHECK(dual_basis({{20, 0, 0}}, {{20 * c30, 20 * s30, 0}}, np, ep));
  const float c29 = std::cos(29.0f * kDegToRad), s29 = std::sin(29.0f * kDegToRad);
  CHECK(!dual_basis({{20, 0, 0}}, {{20 * c29, 20 * s29, 0}}, np, ep));
}

static void test_fit_recovers_heading() {
  std::mt19937 rng(1234);
  Vehicle v;
  ObservationGrid grid = drive(v, rng);
  CHECK(grid.stats().samples == 40);
  CHECK(grid.stats().sectors == 8);
  Calibration cal;
  const FitResult r = try_fit(grid, FitRules{}, 0, 0.0f, 1.5f, -0.5f, cal);
  CHECK(r == FitResult::ACCEPTED);
  CHECK(cal.samples == 40.0f);
  CHECK(cal.pitch_offset == 1.5f && cal.roll_offset == -0.5f);
  CHECK(cal.usable_for(1.5f, -0.5f));
  CHECK(!cal.usable_for(1.5f, 0.0f));  // level changed -> the fit is not valid for it
  CHECK(!cal.usable_for(0.0f, -0.5f));
  // Hard iron recovered (to within the noise and the weakly constrained gravity terms).
  for (size_t i = 0; i < 3; ++i)
    CHECK_NEAR(cal.center[i], v.c0[i], 1.0);
  const float err = max_heading_error(v, cal, rng);
  std::printf("  max heading error with 5 tilts, noise 0.3: %.2f deg\n", err);
  CHECK(err < 2.0f);
}

static void test_fit_with_sparser_drive_and_more_noise() {
  std::mt19937 rng(99);
  Vehicle v;
  v.noise = 1.0f;
  ObservationGrid grid = drive(v, rng, 20.0f, 3);  // level + nose up/down only, coarse
  Calibration cal;
  const FitResult r = try_fit(grid, FitRules{}, 0, 0.0f, 0.0f, 0.0f, cal);
  CHECK(r == FitResult::ACCEPTED);
  const float err = max_heading_error(v, cal, rng);
  std::printf("  max heading error with 3 tilts, noise 1.0: %.2f deg\n", err);
  CHECK(err < 8.0f);  // coarse drive, no roll variation, 3x the noise: a few degrees is expected
}

static void test_level_drive_only() {
  // A drive on flat ground gives no slope variation: the ridge keeps the gravity terms at ~0 and the fit still works
  // for level headings.
  std::mt19937 rng(5);
  Vehicle v;
  ObservationGrid grid = drive(v, rng, 5.0f, 1);
  Calibration cal;
  CHECK(try_fit(grid, FitRules{}, 0, 0.0f, 0.0f, 0.0f, cal) == FitResult::ACCEPTED);
  Pose level;
  compute_pose(0, 0, level);
  for (float course = 0.0f; course < 360.0f; course += 13.0f) {
    const float h = magnetic_heading(cal, v.field(course, level, rng), level);
    CHECK(std::abs(angle_diff(h, course)) < 1.5f);
  }
}

static void test_rejections() {
  std::mt19937 rng(42);
  Vehicle v;
  Calibration cal, untouched;
  const FitRules rules;

  // Too few sectors: only 4 of 8 (alternate sectors) even with plenty of samples.
  {
    ObservationGrid grid;
    Pose pose;
    compute_pose(0, 0, pose);
    for (float course : {0.0f, 90.0f, 180.0f, 270.0f})
      for (const Tilt &t : kTilts) {
        compute_pose(t.pitch, t.roll, pose);
        grid.add(course, pose, v.field(course, pose, rng));
      }
    CHECK(grid.stats().sectors == 4);
    CHECK(try_fit(grid, rules, 0, 0.0f, 0, 0, cal) == FitResult::FEW_SECTORS);
  }
  // Five sectors but all on one side: gap 4 > 3.
  {
    ObservationGrid grid;
    Pose pose;
    for (float course : {0.0f, 45.0f, 90.0f, 135.0f, 180.0f})
      for (const Tilt &t : kTilts) {
        compute_pose(t.pitch, t.roll, pose);
        grid.add(course, pose, v.field(course, pose, rng));
      }
    CHECK(try_fit(grid, rules, 0, 0.0f, 0, 0, cal) == FitResult::SECTOR_GAP);
    // The same grid passes when the gap rule allows it (it is an option of the component).
    FitRules loose;
    loose.max_sector_gap = 4;
    CHECK(try_fit(grid, loose, 0, 0.0f, 0, 0, cal) == FitResult::ACCEPTED);
  }
  // Not enough new samples since the last fit of this session / fewer than the saved fit.
  ObservationGrid full = drive(v, rng);
  CHECK(try_fit(full, rules, 40, 0.0f, 0, 0, cal) == FitResult::FEW_NEW_SAMPLES);
  CHECK(try_fit(full, rules, 36, 0.0f, 0, 0, cal) == FitResult::ACCEPTED);
  CHECK(try_fit(full, rules, 35, 0.0f, 0, 0, cal) == FitResult::ACCEPTED);
  CHECK(try_fit(full, rules, 37, 0.0f, 0, 0, cal) == FitResult::FEW_NEW_SAMPLES);
  CHECK(try_fit(full, rules, 0, 41.0f, 0, 0, cal) == FitResult::FEWER_THAN_SAVED);
  CHECK(try_fit(full, rules, 0, 40.0f, 0, 0, cal) == FitResult::ACCEPTED);
  // A rejected fit leaves the output untouched.
  CHECK(try_fit(full, rules, 40, 0.0f, 3, 4, untouched) == FitResult::FEW_NEW_SAMPLES);
  CHECK(untouched.samples == 0.0f && untouched.pitch_offset == 0.0f);

  // Collinear axes: the sensor sees "north" and "east" along (almost) the same vector.
  {
    Vehicle w = v;
    w.b = {{-0.5f * v.a[0], -0.5f * v.a[1], -0.5f * v.a[2] + 0.2f}};
    ObservationGrid grid = drive(w, rng);
    CHECK(try_fit(grid, rules, 0, 0.0f, 0, 0, cal) == FitResult::DEGENERATE_AXES);
  }
  // Horizontal field too weak (|a|^2 < 64, e.g. a magnetometer scaled in the wrong unit).
  {
    Vehicle w = v;
    for (size_t i = 0; i < 3; ++i) {
      w.a[i] *= 0.1f;
      w.b[i] *= 0.1f;
    }
    w.noise = 0.01f;
    ObservationGrid grid = drive(w, rng);
    CHECK(try_fit(grid, rules, 0, 0.0f, 0, 0, cal) == FitResult::DEGENERATE_AXES);
  }
  // Large residual: a distortion the model cannot represent (soft iron, a magnet that moves, interference).
  {
    Vehicle w = v;
    w.second_harmonic = 0.5f;
    ObservationGrid grid = drive(w, rng);
    CHECK(try_fit(grid, rules, 0, 0.0f, 0, 0, cal) == FitResult::LARGE_RESIDUAL);
    w.second_harmonic = 0.05f;  // mild: accepted
    ObservationGrid mild = drive(w, rng);
    CHECK(try_fit(mild, rules, 0, 0.0f, 0, 0, cal) == FitResult::ACCEPTED);
  }
  // One bad sample (a car passing a truck) is enough to fail the residual gate.
  {
    ObservationGrid grid;
    Pose pose;
    for (const Tilt &t : kTilts) {
      compute_pose(t.pitch, t.roll, pose);
      for (float course = 0.0f; course < 360.0f; course += 5.0f)
        grid.add(course, pose, v.field(course, pose, rng));
    }
    compute_pose(0, 0, pose);
    Vec3 bad = v.field(100.0f, pose, rng);
    bad[0] += 40.0f;
    grid.add(100.0f, pose, bad);
    CHECK(try_fit(grid, rules, 0, 0.0f, 0, 0, cal) == FitResult::LARGE_RESIDUAL);
  }
  // Singular: a single observation cell (rules relaxed so the sector rules do not stop it first).
  {
    FitRules relaxed;
    relaxed.min_sectors = 1;
    relaxed.max_sector_gap = 8;
    ObservationGrid grid;
    Pose pose;
    compute_pose(0, 0, pose);
    grid.add(10.0f, pose, v.field(10.0f, pose, rng));
    CHECK(try_fit(grid, relaxed, 0, 0.0f, 0, 0, cal) == FitResult::SINGULAR);
  }
  // Too-extreme tilt never reaches the grid: the pose is refused.
  Pose p;
  CHECK(!compute_pose(70.0f, 10.0f, p));
}

static void test_heading_rejects_implausible_field() {
  std::mt19937 rng(3);
  Vehicle v;
  v.noise = 0.0f;
  Calibration cal;
  ObservationGrid grid = drive(v, rng);
  CHECK(try_fit(grid, FitRules{}, 0, 0.0f, 0, 0, cal) == FitResult::ACCEPTED);
  Pose p;
  compute_pose(0, 0, p);
  const Vec3 field = v.field(123.0f, p, rng);
  CHECK_NEAR(magnetic_heading(cal, field, p), 123.0f, 0.3);
  // The field scaled by 2.5 (a magnet nearby): strength 2.5 > 1.6 -> NAN; scaled by 0.3 (shielded) -> NAN.
  Vec3 strong = field, weak = field;
  for (size_t i = 0; i < 3; ++i) {
    strong[i] = cal.center[i] + (field[i] - cal.center[i]) * 2.5f;
    weak[i] = cal.center[i] + (field[i] - cal.center[i]) * 0.3f;
  }
  CHECK(std::isnan(magnetic_heading(cal, strong, p)));
  CHECK(std::isnan(magnetic_heading(cal, weak, p)));
  Vec3 ok = field;
  for (size_t i = 0; i < 3; ++i)
    ok[i] = cal.center[i] + (field[i] - cal.center[i]) * 1.3f;
  CHECK(std::isfinite(magnetic_heading(cal, ok, p)));
  // Result is always in [0, 360).
  for (float course = 0.0f; course < 360.0f; course += 3.0f) {
    const float h = magnetic_heading(cal, v.field(course, p, rng), p);
    CHECK(h >= 0.0f && h < 360.0f);
  }
}

static void test_field_filter() {
  FieldFilter f;
  CHECK(!f.fresh(0, 1500));
  CHECK(f.update({{10, 0, 0}}, 1000, 0.35f, 2.0f, 1500));
  CHECK(f.fresh(2000, 1500) && !f.fresh(2501, 1500));
  CHECK_NEAR(f.field[0], 10.0f, 1e-6);  // first sample seeds
  CHECK(f.update({{20, 0, 0}}, 1200, 0.35f, 2.0f, 1500));
  CHECK_NEAR(f.field[0], 13.5f, 1e-5);  // EMA 0.35
  CHECK_NEAR(f.raw[0], 20.0f, 1e-6);    // the fit gets the unfiltered sample
  CHECK(f.update({{30, 0, 0}}, 3000, 0.35f, 2.0f, 1500));
  CHECK_NEAR(f.field[0], 30.0f, 1e-6);  // gap > 1.5 s reseeds
  CHECK(!f.update({{1, 1, 0}}, 3100, 0.35f, 2.0f, 1500));  // |B|^2 = 2 < 4: rejected
  CHECK(!f.ready);
  CHECK(!f.fresh(3100, 1500));
  CHECK_NEAR(f.raw[0], 30.0f, 1e-6);  // rejected sample did not overwrite
  CHECK(!f.update({{NAN, 0, 0}}, 3200, 0.35f, 2.0f, 1500));
  CHECK(f.update({{2, 0, 0}}, 3300, 0.35f, 2.0f, 1500));  // |B|^2 = 4: accepted (not < 4)
  CHECK_NEAR(f.field[0], 2.0f, 1e-6);  // reseeded because not ready
}

static void test_policy() {
  CHECK(select_heading(10.0f, 20.0f, 30.0f).source == Source::GPS);
  CHECK(select_heading(NAN, 20.0f, 30.0f).source == Source::MAGNETIC);
  CHECK_NEAR(select_heading(NAN, 20.0f, 30.0f).heading, 20.0f, 0);
  CHECK(select_heading(NAN, NAN, 30.0f).source == Source::LAST_GPS);
  CHECK(select_heading(NAN, NAN, NAN).source == Source::NONE);
  CHECK(std::isnan(select_heading(NAN, NAN, NAN).heading));
  CHECK(select_heading(0.0f, 20.0f, 30.0f).source == Source::GPS);  // 0 deg is a valid heading

  // learning_status(done, saved, fixed, moving, speed, min_speed, hdop_ok, field_fresh)
  CHECK(learning_status(false, false, false, false, NAN, 18, false, false) == Status::WAITING_FOR_FIX);
  CHECK(learning_status(false, false, true, false, 0, 18, true, true) == Status::WAITING_FOR_MOTION);
  CHECK(learning_status(false, false, true, true, 12, 18, true, true) == Status::WAITING_FOR_MOTION);
  CHECK(learning_status(false, false, true, true, 40, 18, false, true) == Status::CHECK_RECEPTION);
  CHECK(learning_status(false, false, true, true, 40, 18, true, false) == Status::WAITING_FOR_MAGNETOMETER);
  CHECK(learning_status(false, false, true, true, 40, 18, true, true) == Status::COLLECTING);
  CHECK(learning_status(false, true, false, false, NAN, 18, false, false) == Status::SAVED_FIT);  // wins over all
  CHECK(learning_status(true, true, true, true, 40, 18, true, true) == Status::CALIBRATED);
  CHECK(learning_status(true, false, false, false, NAN, 18, false, false) == Status::CALIBRATED);
}

static void test_parse_number() {
  CHECK(std::isnan(parse_number("")));  // an empty RMC course is "unknown", not 0
  CHECK(std::isnan(parse_number(nullptr)));
  CHECK(std::isnan(parse_number("abc")));
  CHECK(std::isnan(parse_number("12.5x")));
  CHECK(std::isnan(parse_number("nan")));
  CHECK_NEAR(parse_number("12.5"), 12.5f, 1e-6);
  CHECK_NEAR(parse_number("0"), 0.0f, 0);
  CHECK_NEAR(parse_number("359.99"), 359.99f, 1e-3);
  CHECK_NEAR(normalize_degrees(-90.0f), 270.0f, 1e-5);
  CHECK_NEAR(normalize_degrees(540.0f), 180.0f, 1e-5);
  CHECK(normalize_degrees(360.0f) == 0.0f);
  CHECK(std::isnan(normalize_degrees(NAN)));
}

int main() {
  struct {
    const char *name;
    void (*fn)();
  } tests[] = {
      {"pose", test_pose},
      {"tilt matches the Tab5 reference formulas", test_tilt_matches_reference_formulas},
      {"moving average", test_moving_average},
      {"observation grid", test_grid},
      {"dual basis", test_dual_basis},
      {"fit recovers heading", test_fit_recovers_heading},
      {"sparser drive, more noise", test_fit_with_sparser_drive_and_more_noise},
      {"level-only drive", test_level_drive_only},
      {"rejections", test_rejections},
      {"implausible field", test_heading_rejects_implausible_field},
      {"field filter", test_field_filter},
      {"source and status policy", test_policy},
      {"number parsing", test_parse_number},
  };
  for (const auto &t : tests) {
    const int before = g_failed;
    std::printf("[ RUN ] %s\n", t.name);
    t.fn();
    std::printf("[ %s ] %s\n", g_failed == before ? " OK " : "FAIL", t.name);
  }
  std::printf("%d checks, %d failed\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
