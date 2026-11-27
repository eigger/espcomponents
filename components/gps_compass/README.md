# gps_compass

[Installation](#installation) · [How it works](#how-it-works) · [Quick start](#quick-start) ·
[Tab5 / callback-only magnetometer](#magnetometer-that-only-exposes-a-callback-m5stack-tab5) ·
[Configuration](#configuration) · [Sensors](#sensors) · [Actions](#actions) · [Triggers](#triggers) ·
[C++ API](#c-api-for-lambdas) · [Calibration procedure](#calibration-procedure) · [Limits](#limits)

A calibrated **true-north heading** for a vehicle from a magnetometer, an accelerometer and a GPS. The magnetometer
calibration is **learned from the GPS course while you drive**, so there is no figure-8 / rotate-the-device calibration.
While the vehicle moves the heading simply is the GPS course; when it is slow or stopped the learned calibration keeps a
magnetic heading alive; just after stopping the last GPS heading is held.

Pure math lives in the header-only `compass_math.h` (no ESPHome includes) and is unit-tested on the host. Tested with
ESPHome 2026.11.0 on ESP32 (ESP-IDF).

## When to use it (versus the `bmm150` heading)

| | `bmm150` `heading` | `gps_compass` |
| --- | --- | --- |
| Calibration | Explicit rotation procedure (`bmm150.calibrate`) | Learned by driving; nothing to do |
| Reference | Magnetic north + declination you configure | True north (GPS course), declination is implicit |
| Hard iron of the installed vehicle | Only if you calibrate in place | Learned in place |
| Pitch / roll of the mounting | Tilt compensation from accel | Learned coupling, plus a settable level |
| Needs GPS | No | Yes (the `gps` component) |
| Without a GPS fix | Yes | Magnetic heading keeps working from the saved fit; learning needs a fix |
| Magnetometer | The `bmm150` driver only | Any source (sensors or a callback) |

Use `bmm150` for a bench compass without GPS; use `gps_compass` in a vehicle with a GPS, or when the magnetometer sits
behind something else (for example the BMM150 on the Tab5's BMI270 aux bus).

## Installation

```yaml
external_components:
  - source: github://eigger/espcomponents@master
    components: [ gps_compass ]
```

Requires the `gps` component (and therefore a `uart`).

## How it works

Per magnetometer axis the field is modelled as

```
field = c0 + a * (forward*N) + b * (right*E - forward_right*N) + g1 * down_forward + g2 * down_right
```

`N`/`E` are the north/east components of the GPS course; `forward`, `right`, `forward_right` project the horizontal axes
onto the tilted vehicle, and `down_forward` / `down_right` are the gravity direction in the vehicle frame (from the
accelerometer, after the level offset). `c0` is the hard iron, `a`/`b` the horizontal field as seen by the sensor, and
`g1`/`g2` how pitch and roll change the reading. The magnetometer axes can be in any orientation: only the
accelerometer axes have to be told (`forward_axis`, `right_axis`).

* **Observations.** One raw field sample is kept for each of 8 course sectors (45 degrees) times 5 slope classes (level,
  nose down/up, right/left down), at most one every 900 ms, only while moving faster than `min_learning_speed` with a good
  fix, fresh sensors and not reversing. A single road direction or camber cannot dominate the fit.
* **Fit.** At most every 3 s a 5x5 least squares (small ridge on the two gravity terms) is solved once at least
  `min_sectors` (5) sectors are covered with no gap wider than `max_sector_gap` (3), and at least `min_new_samples` (4)
  more samples exist than at the last fit. A saved fit is never replaced by one made from fewer samples.
* **Acceptance.** The horizontal field vectors `a`, `b` must be large (each |.|^2 >= 64, i.e. 8 units) and not collinear
  (`det >= 0.25 * |a|^2 * |b|^2`), and every observation must be within `err^2 <= 0.12 * min(|a|^2, |b|^2)` of the model.
* **Heading.** The measured field is centred, the gravity terms removed, and projected onto the dual basis to get the
  north and east components; the heading is `atan2(east, north)` and is accepted only when `hypot(north, east)` is
  between 0.4 and 1.6.
* **Level.** The fit stores the pitch/roll offsets it was made with. Change the level and the saved fit is not valid any
  more (until you change it back): learning restarts. It is stored, so it survives reboots.
* **Source selection.** Live GPS course while moving (plus 180 degrees when `reverse`), else the magnetic heading, else
  the GPS heading from just before the vehicle stopped, else none.

## Quick start

GPS on a UART, a magnetometer and an accelerometer as ordinary sensors:

```yaml
uart:
  rx_pin: GPIO16
  tx_pin: GPIO17
  baud_rate: 9600

gps:
  id: car_gps
  update_interval: 2s

gps_compass:
  id: compass
  gps_id: car_gps
  magnetometer: { x: mag_x, y: mag_y, z: mag_z }        # sensor ids, microtesla
  accelerometer: { x: acc_x, y: acc_y, z: acc_z }       # sensor ids, any unit
  forward_axis: +Y        # which accelerometer axis points to the nose
  right_axis: +X          # ... and to the right-hand side
  heading:
    name: Heading
  source:
    name: Heading source   # none / gps / magnetic / last_gps
  calibrated:
    name: Compass calibrated
```

## Magnetometer that only exposes a callback (M5Stack Tab5)

On the Tab5 the BMM150 sits behind the BMI270's auxiliary bus (the `bmi270` component's motion hub) and is exposed only
as `add_magnetometer_listener`; there is no sensor entity. Feed the component from the callback with `update_field()`
(`update_accel()` is the same for an accelerometer):

```yaml
esphome:
  on_boot:
    - priority: -100
      then:
        - lambda: |-
            id(gnss_imu).add_magnetometer_listener([](const bmi270::bmm150::BMM150Data &data) {
              id(compass).update_field(data.x, data.y, data.z);
            });

gps_compass:
  id: compass
  gps_id: id_gps
  accelerometer: { x: id_bmi270_accel_x, y: id_bmi270_accel_y, z: id_bmi270_accel_z }
  # The Tab5 reference device: pitch = atan2(-az, hypot(ax, ay)) with direction -1, roll = atan2(-ay, ...) with +1.
  forward_axis: +Z
  right_axis: +Y
  heading: { name: Heading, id: heading }
  pitch: { name: Pitch }
  roll: { name: Roll }
```

The smoothing (EMA 0.35), the reseed after a 1.5 s gap and the rejection of samples with |B|^2 < 4 are done inside
`update_field()`, so the callback just forwards the sample. The raw sample is what the fit uses.

## Configuration

| Option | Default | Description |
| --- | --- | --- |
| `gps_id` | required | The `gps` component. Registers a `GPSListener` and reads `$GNRMC/GPRMC/GARMC/GBRMC/GLRMC` through `TinyGPSCustom` (an empty course is "unknown", not 0). Satellites, HDOP and location validity come from TinyGPSPlus |
| `magnetometer` | — | `{x, y, z}` sensor ids. Omit when you call `update_field()` |
| `accelerometer` | — | `{x, y, z}` sensor ids. Omit when you call `update_accel()` |
| `forward_axis` | `+Y` | Accelerometer axis (`X`/`Y`/`Z`, optional `+`/`-`) along the vehicle's nose |
| `right_axis` | `+X` | Accelerometer axis along the vehicle's right side. Must differ from `forward_axis` |
| `pitch_offset` / `roll_offset` | `0` | Initial level in degrees (raw reading that counts as level). A stored level (from `set_level` / `clear_level`) overrides it |
| `reverse` | `false` | Templatable bool. While true, 180 degrees are added to the GPS heading and learning is paused |
| `stationary.moving_signal` | `false` | Templatable bool for a supplemental movement signal such as OBD speed. A true value bypasses the stationary magnetic heading deadband. |
| `stationary.deadband` | `0°` | Hold magnetic heading while its circular change is below this angle. Zero disables the filter. Live GPS heading bypasses it. |
| `min_learning_speed` | `18 km/h` | Observations are taken above this speed |
| `moving_speed` | `8 km/h` | Above this the vehicle is "moving" (live GPS heading) |
| `stopped_speed` | `3 km/h` | GPS speed below this counts as stopped. A valid speed at or above this value bypasses the stationary heading deadband. |
| `max_hdop` | `3.0` | Observations need `0 < HDOP <= max_hdop` |
| `min_satellites` | `4` | Fix needs at least this many satellites |
| `field_filter` | `0.35` | EMA factor of the magnetic heading input (1 = no smoothing) |
| `min_field_strength` | `2.0` | Samples with a smaller magnitude are dropped (sensor not ready). Unit of the magnetometer; thresholds assume microtesla |
| `sensor_timeout` | `1500ms` | Magnetometer / accelerometer freshness, and the gap after which the field filter reseeds |
| `gps_timeout` | `3s` | RMC / location / satellites / HDOP freshness |
| `observation_interval` | `900ms` | Minimum time between observations |
| `fit_interval` | `3s` | Minimum time between fit attempts |
| `min_sectors` | `5` | Course sectors (of 8) a fit needs |
| `max_sector_gap` | `3` | Widest gap, in sectors, between covered sectors |
| `min_new_samples` | `4` | New samples required over the last fit of this session |
| `save.max_per_boot` | `0` | Maximum automatic flash save attempts per boot. Positive limits sync eligible fits immediately. Zero uses the regular preferences sync without a per-boot limit. |
| `save.min_change` | `0.5°` | With a positive save limit, compare the saved and new fit on the same observations. Save when the heading differs by at least this angle or when the previous fit cannot supply a valid heading. |

```yaml
gps_compass:
  gps_id: id_gps
  save:
    max_per_boot: 1
    min_change: 0.5 deg
  stationary:
    deadband: 1 deg
```

Fixed (see `compass_math.h`): speed is `RMC knots * 1.852`; an observation needs an RMC younger than 750 ms and a
magnetometer sample younger than 500 ms; tilt beyond about 60 degrees is refused; ridge 0.006; the acceptance numbers
above.

## Sensors

All optional.

| Key | Description |
| --- | --- |
| `heading` | Selected heading, degrees `[0, 360)`, true north; unknown when no source. Published when the rounded degree changes |
| `magnetic_heading` | Calibrated magnetic heading regardless of the selected source |
| `gps_heading` | Live GPS heading while moving (with the reverse correction) |
| `pitch` / `roll` | Leveled attitude in degrees (nose-up / right-side-down positive), 5-sample moving average, 200 ms sampling, unknown when the accelerometer is stale (> 3 s) |
| `samples` | Observation cells collected since boot / level change / reset (max 40) |
| `source` (text) | `none`, `gps`, `magnetic`, `last_gps` |
| `status` (text) | `waiting_for_fix`, `waiting_for_motion`, `check_reception`, `waiting_for_magnetometer`, `collecting`, `saved_fit` (a saved fit is in use, learning continues), `calibrated` (a fit was accepted) |
| `calibrated` (binary) | A fit made at the current level exists (restored or learned) |

## Actions

| Action | Description |
| --- | --- |
| `gps_compass.set_level` | No options: the current (smoothed) pose becomes level. `pitch:` / `roll:` (templatable, degrees): set those raw offsets, keeping an omitted one |
| `gps_compass.clear_level` | Level back to 0 / 0 |
| `gps_compass.reset_calibration` | Forget the fit and the collected samples (level is kept) |

Changing the level saves it and makes the previously learned fit invalid until the level returns; learning restarts. A
UI "level" button that toggles is `if: condition: lambda: "return id(compass).pitch_offset() != 0;" then: clear_level
else: set_level`.

## Triggers

`on_calibrated` fires when a fit is accepted: `first` (bool) is true for the first fit since boot / level change /
reset, false for later improved fits. `samples` (uint32) is the number of observations in the fit. `saved` (bool)
confirms an immediate flash sync for this fit when `save.max_per_boot` is positive.

```yaml
gps_compass:
  on_calibrated:
    - lambda: ESP_LOGI("compass", "fit %s with %u samples", saved ? "saved" : "accepted", samples);
```

## C++ API (for lambdas)

```cpp
auto &c = id(compass);
c.update_field(x, y, z);  c.update_accel(x, y, z);       // callback sources
c.heading();               // [0,360) or NAN
c.source();                // gps_compass::Source::{NONE,GPS,MAGNETIC,LAST_GPS};  c.source_name()
c.magnetic_heading();  c.gps_heading();  c.stopped_heading();
c.has_fix();  c.is_moving();  c.is_stopped();  c.hdop_ok();  c.is_reverse();  c.speed_kmh();
c.field_fresh();  c.pose_valid();
c.is_calibrated();         // a fit exists for the current level
c.learning_done();         // a fit was accepted since boot / level change / reset
c.sample_count();  c.sector_count();  c.fit_sample_count();
c.status();                // gps_compass::Status::{WAITING_FOR_FIX, WAITING_FOR_MOTION, CHECK_RECEPTION,
                           //   WAITING_FOR_MAGNETOMETER, COLLECTING, SAVED_FIT, CALIBRATED};  c.status_name()
c.pitch();  c.roll();  c.pitch_offset();  c.roll_offset();
c.level_to_current();  c.set_level_offsets(pitch, roll);  c.clear_level();  c.reset_calibration();
```

The dial / status line of a dashboard needs nothing else: `heading()` + `source()` for the needle and its label,
`status()` for the setup text, `learning_done()` for a one-time "calibrated" notice (or `on_calibrated`).

## Calibration procedure

1. Mount the device where it will stay (the hard iron of the vehicle is part of what is learned).
2. Optionally level it: park on flat ground and run `gps_compass.set_level`. Do this **first**: changing the level later
   discards the fit.
3. Drive in **several directions above 18 km/h** (default `min_learning_speed`) with a good fix (HDOP <= 3, at least 4
   satellites): a few turns, a loop around a block is enough. At least 5 of the 8 compass sectors must be covered with no
   gap wider than 3. Reversing and slow driving are ignored. `status` shows `collecting` while it gathers.
4. When a fit is accepted, `status` becomes `calibrated` and `on_calibrated` fires. By default, accepted fits are
   queued for the next preferences sync. Set `save.max_per_boot` to limit automatic writes during a boot.

Slopes help: the pitch/roll coupling is learned from hills and camber; on flat ground only the level heading is
corrected, which is what you normally want.

## Limits

* GPS course is only reliable when moving; the magnetic heading takes over below `moving_speed` and is as good as the
  calibration (a few degrees). It is unknown until the first fit and after a level change.
* Strong or changing local fields (a phone on the dash, speaker magnets, a metal object that moves relative to the
  sensor) invalidate the model; fits with a large residual are rejected and the previous fit is kept.
* True north comes from the GPS course, so declination needs no configuration, but the magnetic heading is only as
  "true" as the drives it was learned from.
* Tilts beyond about 60 degrees from the level (pitch + roll combined) are refused.
* Magnetometer units: the strength thresholds assume microtesla (BMM150 is). Scale other units first.
* Smoothing, not a stability guarantee: pitch/roll are for display and the fit, not for safety use.
* A GPS that emits only GGA (no RMC) gives no speed/course and therefore no learning.
