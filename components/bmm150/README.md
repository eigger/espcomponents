# BMM150

Bosch BMM150 3-axis magnetometer. Publishes raw XYZ and a vehicle heading.

M5Stack Unit GNSS uses address **0x10**. For a compass, set `update_interval` to **200ms–1s**.

## Options

| Key | Default | Description |
|---|---|---|
| `address` | `0x10` | I2C address (0x10–0x13, CSB/SDO strap) |
| `update_interval` | `60s` | Poll period. Use 200ms–1s for heading |
| `magnetic_field_x/y/z` | — | Magnetic field in µT (Bosch integer compensation; **unverified on hardware**) |
| `heading` | — | True heading 0–360°. Stays `unknown` until a valid calibration is stored |
| `accel_x_id` / `accel_y_id` / `accel_z_id` | — | Accel sensors for tilt compensation. If omitted, planar `atan2(-my, mx)` is used. If set but the accel has no valid sample, heading stays `unknown` (no silent planar fallback) |
| `declination` | `0` | Magnetic declination in degrees. Korea is about **-8** (west). Not hardcoded |
| `heading_offset` | `0` | Mounting offset in degrees (module +X vs vehicle forward). Added after declination |
| `soft_iron` | `false` | Per-axis scale correction. Leave off for in-place yaw calibration |
| `calibration_mode` | `yaw` | `yaw`: in-place full circle, XY only. `full`: figure-8 including Z |
| `mag_axes` | `[x, y, z]` | Mag axis remap (`x`,`y`,`z`,`-x`,`-y`,`-z`). **PCB alignment unverified** |
| `accel_axes` | `[x, y, z]` | Accel axis remap. **PCB alignment unverified** |
| `on_calibration_finished` | — | Trigger after calibration. `success` (bool) |

## Calibration

A dashboard has hard-iron offset from steel and speakers. Heading is not published without a stored calibration. Calibrate **in the installed position** so that offset includes the vehicle.

### In the vehicle (`calibration_mode: yaw`, default)

1. Leave the module mounted.
2. Run `bmm150.calibrate` and **slowly drive or turn the vehicle through a full circle in place**.
3. X and Y min/max delta must each exceed **20 µT**. If Z delta also exceeds **5 µT** (typical on a tilted dash), the Z offset is updated; otherwise the previous Z offset is kept.
4. A level mount cannot excite Z by yaw rotation — that is expected.

### Off the vehicle (`calibration_mode: full`)

1. Remove the module and rotate it in a slow **figure-8** so all three axes move.
2. XY delta must exceed **20 µT**, Z delta **5 µT**.
3. This does **not** capture dash-plate hard-iron. Use it only to map axes, then redo `yaw` once reinstalled.

`duration` must cover several `update_interval` samples. With the schema default of 60s and `duration: 30s`, calibration will always be rejected.

Changing `mag_axes` or the internal µT scale invalidates the stored blob; recalibrate.

```yaml
on_...:
  - bmm150.calibrate:
      id: mag
      duration: 30s
```

## Vehicle install

Keep the module away from speaker magnets, steel panels, and high-current wiring. Mounting the GNSS unit on a steel dash can add tens of µT of offset.

If the module is not aligned with vehicle forward, set `heading_offset` (degrees, clockwise from module +X to the nose of the car). Do not reuse `mag_axes` for that — those options are for chip-to-chip axis swap.

## Axis alignment and declination

Tilt compensation assumes mag and accel X/Y/Z point the same way. On M5 Unit GNSS the BMM150 and BMI270 may be rotated relative to each other — measure, then set `mag_axes` / `accel_axes`. Recalibrate after changing those lists.

For true north, set `declination`. Seoul is about `-8`.

## Example

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ bmm150 ]

i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

sensor:
  - platform: bmm150
    id: mag
    address: 0x10
    update_interval: 500ms
    magnetic_field_x:
      name: "MAG X"
    magnetic_field_y:
      name: "MAG Y"
    magnetic_field_z:
      name: "MAG Z"
    heading:
      name: "Heading"
    accel_x_id: id_gnss_bmi270_accel_x
    accel_y_id: id_gnss_bmi270_accel_y
    accel_z_id: id_gnss_bmi270_accel_z
    declination: -8
    heading_offset: 0
    calibration_mode: yaw
    mag_axes: [x, y, z]
    accel_axes: [x, y, z]
    on_calibration_finished:
      - logger.log:
          format: "BMM150 calibration %s"
          args: ['success ? "ok" : "failed"']
```
