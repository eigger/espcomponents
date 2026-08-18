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
| `soft_iron` | `true` | Per-axis scale correction during calibration |
| `mag_axes` | `[x, y, z]` | Mag axis remap (`x`,`y`,`z`,`-x`,`-y`,`-z`). **PCB alignment unverified** |
| `accel_axes` | `[x, y, z]` | Accel axis remap. **PCB alignment unverified** |
| `on_calibration_finished` | — | Trigger after calibration. `success` (bool) |

## Calibration

A dashboard has hard-iron offset from steel and speakers. Heading is not published without a stored calibration.

1. Mount the sensor, then run the action below.
2. During `duration`, rotate the device in a slow **figure-8** so all three axes move.
3. If any axis min/max delta is **below 20 µT**, the result is discarded (`success=false`).
4. On success the offsets are stored in NVS (in flash) and survive reboot.
5. Changing `mag_axes` or the internal µT scale invalidates the stored blob; recalibrate.

`duration` must cover several `update_interval` samples. With the schema default of 60s and `duration: 30s`, calibration will always be rejected.

```yaml
on_...:
  - bmm150.calibrate:
      id: mag
      duration: 30s
```

## Vehicle install

Keep the module away from speaker magnets, steel panels, and high-current wiring. Mounting the GNSS unit on a steel dash can add tens of µT of offset.

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
    mag_axes: [x, y, z]
    accel_axes: [x, y, z]
    on_calibration_finished:
      - logger.log:
          format: "BMM150 calibration %s"
          args: ['success ? "ok" : "failed"']
```
