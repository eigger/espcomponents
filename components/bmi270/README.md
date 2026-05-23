# Standalone BMI270 Sensor Component

[![ESPHome](https://img.shields.io/badge/ESPHome-Custom%20Component-blue)](https://esphome.io/)

A high-performance, **completely standalone** ESPHome custom component for the Bosch **BMI270** 6-axis Inertial Measurement Unit (IMU). 

Unlike other implementations, this component has **zero external framework dependencies** (no separate `motion` component required) and exposes all six telemetry axes (Accelerometer X/Y/Z, Gyroscope X/Y/Z) and Temperature directly to ESPHome as standard individual sensor nodes.

Additionally, it automatically handles the mandatory Bosch binary configuration blob upload (~8KB firmware) over I2C on startup to ensure high precision, calibration, and optimal performance.

---

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Configuration Reference](#configuration-reference)
  - [Component Options](#component-options)
  - [Sensor Blocks](#sensor-blocks)
- [Range and ODR References](#range-and-odr-references)
  - [Accelerometer Range & ODR](#accelerometer-range--odr)
  - [Gyroscope Range & ODR](#gyroscope-range--odr)
- [Orientation Examples](#orientation-examples)
  - [Orientation 1 — Flat (Z-up)](#orientation-1--flat-z-up)
  - [Orientation 2 — Portrait (Y-up)](#orientation-2--portrait-y-up)
  - [Orientation 3 — Landscape (X-up)](#orientation-3--landscape-x-up)
- [I2C Communication & Architecture](#i2c-communication--architecture)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Features

- **No Modular Overheads**: Inherits directly from `PollingComponent` and `i2c::I2CDevice`. All configurations, sensor scaling, and registry operations are encapsulated natively.
- **Efficient Burst Reads**: Reads all 6 telemetry axes and temperature in a single, high-efficiency 18-byte I2C burst read sweep.
- **Full Customizability**: Individually configure the full-scale range and Output Data Rate (ODR) for both the Accelerometer and Gyroscope.
- **Automatic Firmware Load**: Automatically uploads the required Bosch SensorAPI initialization config file in 256-byte pages during setup, verifying status with the internal `INTERNAL_STATUS` registers.

---

## Installation

Add the following to your ESPHome configuration:

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ bmi270 ]
    refresh: always
```

---

## Quick Start

```yaml
i2c:
  sda: 21
  scl: 22
  scan: true
  id: bus_a

sensor:
  - platform: bmi270
    address: 0x68                  # 0x68 (default) or 0x69
    update_interval: 1s
    accel_x:
      name: "BMI270 Accel X"
    accel_y:
      name: "BMI270 Accel Y"
    accel_z:
      name: "BMI270 Accel Z"
    gyro_x:
      name: "BMI270 Gyro X"
    gyro_y:
      name: "BMI270 Gyro Y"
    gyro_z:
      name: "BMI270 Gyro Z"
    temperature:
      name: "BMI270 Temperature"
```

---

## Configuration Reference

### Component Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `address` | integer | `0x68` | I2C address of the sensor (`0x68` when SDO pin is low, `0x69` when SDO is high). |
| `update_interval` | time | `1s` | The frequency at which the component polls sensor data. |
| `accel_range` | string | `4G` | Accelerometer full-scale range (see [Accelerometer Range](#accelerometer-range--odr)). |
| `accel_odr` | string | `100HZ` | Accelerometer Output Data Rate (see [Accelerometer ODR](#accelerometer-range--odr)). |
| `gyro_range` | string | `2000DPS` | Gyroscope full-scale range (see [Gyroscope Range](#gyroscope-range--odr)). |
| `gyro_odr` | string | `200HZ` | Gyroscope Output Data Rate (see [Gyroscope ODR](#gyroscope-range--odr)). |

### Sensor Blocks

All 7 telemetry outputs can be enabled or disabled selectively using standard ESPHome sensor configurations:

- **`accel_x`** (g): X-axis acceleration.
- **`accel_y`** (g): Y-axis acceleration.
- **`accel_z`** (g): Z-axis acceleration.
- **`gyro_x`** (dps): X-axis angular velocity (degrees per second).
- **`gyro_y`** (dps): Y-axis angular velocity.
- **`gyro_z`** (dps): Z-axis angular velocity.
- **`temperature`** (°C): Chip internal temperature.

Each sensor supports all standard ESPHome sensor properties such as `name`, `icon`, `filters`, `accuracy_decimals`, etc.

---

## Range and ODR References

### Accelerometer Range & ODR

- **Ranges**: `2G` (±2g), `4G` (±4g), `8G` (±8g), `16G` (±16g)
- **Output Data Rates (ODR)**: `12.5HZ`, `25HZ`, `50HZ`, `100HZ`, `200HZ`, `400HZ`, `800HZ`, `1600HZ`

Example configuration:
```yaml
sensor:
  - platform: bmi270
    accel_range: 8G
    accel_odr: 400HZ
    accel_x:
      name: "Accel X"
```

### Gyroscope Range & ODR

- **Ranges**: `125DPS` (±125°/s), `250DPS` (±250°/s), `500DPS` (±500°/s), `1000DPS` (±1000°/s), `2000DPS` (±2000°/s)
- **Output Data Rates (ODR)**: `25HZ`, `50HZ`, `100HZ`, `200HZ`, `400HZ`, `800HZ`, `1600HZ`, `3200HZ`

Example configuration:
```yaml
sensor:
  - platform: bmi270
    gyro_range: 500DPS
    gyro_odr: 800HZ
    gyro_x:
      name: "Gyro X"
```

---

## Orientation Examples

The BMI270 is a 6-axis IMU (accelerometer + gyroscope). Roll and Pitch can be derived from accelerometer data using ESPHome's `template` sensor.

> **Note:** BMI270 has no built-in magnetometer, so absolute heading (Yaw/azimuth) cannot be determined from this sensor alone.

### Roll & Pitch from Accelerometer

The lambda formulas for Roll and Pitch depend on how the sensor is physically mounted. Choose the section that matches your device orientation.

The shared sensor block below is the same for all orientations — only the `template` lambdas differ:

```yaml
i2c:
  sda: 21
  scl: 22
  id: bus_a

sensor:
  - platform: bmi270
    address: 0x68
    update_interval: 100ms
    accel_odr: 100HZ
    accel_x:
      id: bmi270_accel_x
      internal: true
    accel_y:
      id: bmi270_accel_y
      internal: true
    accel_z:
      id: bmi270_accel_z
      internal: true
```

---

#### Orientation 1 — Flat (Z-up)

Sensor lying horizontally, Z-axis pointing up (most common PCB-on-table mounting).

```
        Z↑
        |
        +——— X (forward)
       /
      Y (left)
```

```yaml
  - platform: template
    name: "Roll"                       # left/right tilt
    unit_of_measurement: "°"
    accuracy_decimals: 1
    update_interval: 100ms
    lambda: |-
      return atan2(id(bmi270_accel_y).state,
                   id(bmi270_accel_z).state) * 180.0f / M_PI;

  - platform: template
    name: "Pitch"                      # forward/backward tilt
    unit_of_measurement: "°"
    accuracy_decimals: 1
    update_interval: 100ms
    lambda: |-
      float ax = id(bmi270_accel_x).state;
      float ay = id(bmi270_accel_y).state;
      float az = id(bmi270_accel_z).state;
      return atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / M_PI;
```

**Output ranges:** Roll ±180°, Pitch ±90°

---

#### Orientation 2 — Portrait (Y-up)

Sensor standing upright, Y-axis pointing up (e.g., wall-mounted vertically, connector at bottom).

```
        Y↑
        |
        +——— X (forward)
       /
      Z (toward viewer)
```

```yaml
  - platform: template
    name: "Roll"                       # left/right tilt
    unit_of_measurement: "°"
    accuracy_decimals: 1
    update_interval: 100ms
    lambda: |-
      return atan2(id(bmi270_accel_z).state,
                   id(bmi270_accel_y).state) * 180.0f / M_PI;

  - platform: template
    name: "Pitch"                      # forward/backward tilt
    unit_of_measurement: "°"
    accuracy_decimals: 1
    update_interval: 100ms
    lambda: |-
      float ax = id(bmi270_accel_x).state;
      float ay = id(bmi270_accel_y).state;
      float az = id(bmi270_accel_z).state;
      return atan2(-ax, sqrt(az * az + ay * ay)) * 180.0f / M_PI;
```

---

#### Orientation 3 — Landscape (X-up)

Sensor mounted on its side, X-axis pointing up (e.g., side-mounted panel).

```
        X↑
        |
        +——— Z (forward)
       /
      Y (left)
```

```yaml
  - platform: template
    name: "Roll"                       # left/right tilt
    unit_of_measurement: "°"
    accuracy_decimals: 1
    update_interval: 100ms
    lambda: |-
      return atan2(id(bmi270_accel_y).state,
                   id(bmi270_accel_x).state) * 180.0f / M_PI;

  - platform: template
    name: "Pitch"                      # forward/backward tilt
    unit_of_measurement: "°"
    accuracy_decimals: 1
    update_interval: 100ms
    lambda: |-
      float ax = id(bmi270_accel_x).state;
      float ay = id(bmi270_accel_y).state;
      float az = id(bmi270_accel_z).state;
      return atan2(-az, sqrt(ay * ay + ax * ax)) * 180.0f / M_PI;
```

---

## I2C Communication & Architecture

1. **Initialization Sequence**:
   - Soft-resets the BMI270 chip.
   - De-activates advanced power-saving settings to prepare registers.
   - Arming `INIT_CTRL` config loader registry and loading the Bosch firmware configuration file in 256-byte bursts.
   - Restores power configurations, confirms successful load status (`INTERNAL_STATUS` should return `0x01`).
   - Sets configured full-scale ranges and ODR registers.
2. **Reading Telemetry**:
   - Performs a single, atomic 18-byte I2C block read starting from the `DATA_8` accelerometer output register.
   - Extracts signed 16-bit raw registers for the Accelerometer (6 bytes), Gyroscope (6 bytes), Temperature (2 bytes) along with trailing padding buffers.
   - Automatically maps signed values to physical units using scale factors corresponding to configured ranges (e.g. dividing by `32768.0` and multiplying by full range).

---

## Troubleshooting

### Check Hardware & I2C Wiring
Make sure SDA/SCL lines are properly connected and have pull-up resistors (typically 4.7kΩ or 10kΩ).

Enable debug logs to diagnose connection or configuration problems:
```yaml
logger:
  level: DEBUG
  logs:
    bmi270: DEBUG
```

### Common Log Issues

- **`Failed to read chip ID - check wiring / address`**:
  No I2C communication. Check address setting (`0x68` vs `0x69`) or run an I2C scan (`scan: true` under `i2c:` config block).
- **`Wrong chip ID: 0xXX (expected 0x24)`**:
  The chip returned a different identifier. Make sure you aren't pointing at a BMI160 or other device sharing the I2C bus.
- **`Config file upload failed`**:
  Initialization configuration load did not finalize correctly. Check that power supplies are clean and I2C pullups are adequate for sustained page-writes.

---

## License

MIT License — [@eigger](https://github.com/eigger)
