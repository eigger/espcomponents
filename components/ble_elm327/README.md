# BLE ELM327 Component

[![ESPHome](https://img.shields.io/badge/ESPHome-Custom%20Component-blue)](https://esphome.io/)

> [!WARNING]
> **Experimental / Unverified**: This custom component is experimental and has not been extensively verified. It may not function correctly on all devices, ESPHome versions, or vehicle configurations. Use with caution at your own risk.

A custom ESPHome component that connects to a Bluetooth LE ELM327 OBD-II adapter and exposes vehicle data (RPM, speed, temperature, odometer, gear position, etc.) as Home Assistant sensors.

Supports standard OBD-II (Mode 01) and vendor-extended UDS PIDs (Mode 22, e.g. GM/Chevrolet Colorado).

The component registers as a node under ESPHome's standard `ble_client:` component.

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Configuration Reference](#configuration-reference)
  - [Core Component](#core-component)
  - [Device Schema](#device-schema)
- [Platforms](#platforms)
  - [Sensor](#sensor)
- [Presets](#presets)
- [Response Parsing](#response-parsing)
- [Formula Lambda](#formula-lambda)
- [Common OBD-II PIDs](#common-obd-ii-pids)
- [GM Extended PIDs (Mode 22)](#gm-extended-pids-mode-22)
- [Troubleshooting](#troubleshooting)

---

## Installation

Add the following to your ESPHome configuration:

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ ble_elm327 ]
    refresh: always
```

---

## Quick Start

```yaml
esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: obd_client

ble_elm327:
  ble_client_id: obd_client

sensor:
  - platform: ble_elm327
    name: "Engine RPM"
    pid: "0C"
    mode: "01"
    update_interval: 1s
    formula: "return (a * 256.0f + b) / 4.0f;"
    unit_of_measurement: "rpm"

  - platform: ble_elm327
    name: "Vehicle Speed"
    pid: "0D"
    mode: "01"
    update_interval: 1s
    formula: "return a;"
    unit_of_measurement: "km/h"
```

---

## Configuration Reference

### Core Component

```yaml
ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: obd_client

ble_elm327:
  id: elm
  ble_client_id: obd_client
  service_uuid: "FFF0"
  rx_char_uuid: "FFF1"
  tx_char_uuid: "FFF2"
  init_commands:
    - "ATZ"
    - "ATE0"
    - "ATL0"
    - "ATSP0"
  tx_delay: 50
```

#### Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ble_client_id` | ID | **Required** | The ID of the parent `ble_client` component |
| `service_uuid` | UUID | `FFF0` | BLE service UUID of the ELM327 adapter |
| `rx_char_uuid` | UUID | `FFF1` | BLE notify characteristic UUID (adapter → ESP32) |
| `tx_char_uuid` | UUID | `FFF2` | BLE write characteristic UUID (ESP32 → adapter) |
| `init_commands` | list | `[ATZ, ATE0, ATL0, ATSP0]` | AT commands sent on connection. Use `[]` to skip. |
| `tx_delay` | int (ms) | `50` | Minimum delay between consecutive BLE writes |

#### UUID Formats

All UUID fields accept 16-bit, 32-bit, or 128-bit formats:

```yaml
service_uuid: "FFF0"                                    # 16-bit
service_uuid: "0000FFF0"                                # 32-bit
service_uuid: "0000fff0-0000-1000-8000-00805f9b34fb"   # 128-bit
```

#### Init Commands

The default sequence resets the adapter and configures it for silent operation:

| Command | Effect |
|---------|--------|
| `ATZ` | Reset ELM327 |
| `ATE0` | Echo off |
| `ATL0` | Linefeeds off |
| `ATS0` | Spaces off — compact hex responses (`"41051A"` instead of `"41 05 1A"`) |
| `ATSP0` | Auto-detect OBD protocol |

To skip initialisation entirely (e.g. adapter is already configured):

```yaml
ble_elm327:
  ble_client_id: obd_client
  init_commands: []
```

Custom init sequence example (CAN 500 kbps, headers on):

```yaml
ble_elm327:
  ble_client_id: obd_client
  init_commands:
    - "ATZ"
    - "ATE0"
    - "ATL0"
    - "ATSP6"    # ISO 15765-4 CAN (11-bit, 500 kbps)
    - "ATH0"     # Headers off
```

#### Internal State Machine

```
IDLE → (BLE connect + service discovery + notify registered)
     → CONNECTED: init_commands queued into tx_queue
     → loop() drains tx_queue at tx_delay intervals
     → READY
```

In **READY** state, `loop()` collects per-device poll requests and drains them with `tx_delay` between each write — commands are sent without waiting for a response. When a response arrives it is broadcast to **all** registered devices; each device checks whether the mode + PID bytes match its own configuration and updates its state if they do.

Init commands are sent with the same `tx_delay` timing as sensor commands. Responses to AT commands during init are silently ignored (they don't match any sensor's mode + PID).

---

### Device Schema

All sub-platforms (sensor, …) share these options:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ble_elm327_id` | id | auto | ID of the parent `ble_elm327` component. Can be omitted when there is only one `ble_elm327` in the config. |
| `preset` | string | - | Built-in OBD-II preset name (see [Presets](#presets)). Mutually exclusive with `pid`. |
| `pid` | string | **Required** (unless `preset`) | OBD-II PID hex string — 2 chars for mode 01, 4 chars for mode 22 |
| `mode` | string | `"01"` | OBD-II service mode (`"01"` standard, `"22"` UDS/extended) |
| `formula` | lambda | - | Custom value parser (see [Formula Lambda](#formula-lambda)) |
| `update_interval` | time | `60s` | Per-sensor polling interval |

> **Note**: Each sensor has an **independent** `update_interval`. Requests from all sensors are queued and sent one at a time with `tx_delay` between them.

---

## Platforms

### Sensor

```yaml
sensor:
  - platform: ble_elm327
    name: "Engine RPM"
    pid: "0C"
    mode: "01"
    update_interval: 1s
    formula: "return (a * 256.0f + b) / 4.0f;"
    unit_of_measurement: "rpm"
    state_class: measurement
```

All standard ESPHome sensor options (`unit_of_measurement`, `device_class`, `state_class`, `accuracy_decimals`, `filters`, …) are supported.

---

## Presets

Standard OBD-II Mode 01 PIDs are built in. Use `preset:` instead of specifying `pid`, `mode`, `formula`, `unit_of_measurement`, `device_class`, `state_class`, and `accuracy_decimals` manually.

```yaml
sensor:
  - platform: ble_elm327
    name: "Engine RPM"
    preset: rpm
    update_interval: 1s

  - platform: ble_elm327
    name: "Vehicle Speed"
    preset: speed
    update_interval: 1s

  - platform: ble_elm327
    name: "Coolant Temperature"
    preset: coolant_temp
    update_interval: 10s
```

You can still override any individual field when using a preset:

```yaml
sensor:
  - platform: ble_elm327
    name: "Fuel Level"
    preset: fuel_level
    update_interval: 60s
    accuracy_decimals: 0   # override the preset default of 1
```

### Available Presets

#### Engine

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `engine_load` | `04` | `%` | `return a / 2.55f;` |
| `coolant_temp` | `05` | `°C` | `return a - 40.0f;` |
| `short_term_fuel_trim_b1` | `06` | `%` | `return (a * 100.0f / 128.0f) - 100.0f;` |
| `long_term_fuel_trim_b1` | `07` | `%` | `return (a * 100.0f / 128.0f) - 100.0f;` |
| `short_term_fuel_trim_b2` | `08` | `%` | `return (a * 100.0f / 128.0f) - 100.0f;` |
| `long_term_fuel_trim_b2` | `09` | `%` | `return (a * 100.0f / 128.0f) - 100.0f;` |
| `fuel_pressure` | `0A` | `kPa` | `return a * 3.0f;` |
| `intake_pressure` | `0B` | `kPa` | `return a;` |
| `rpm` | `0C` | `rpm` | `return (a * 256.0f + b) / 4.0f;` |
| `speed` | `0D` | `km/h` | `return a;` |
| `timing_advance` | `0E` | `°` | `return a / 2.0f - 64.0f;` |
| `intake_air_temp` | `0F` | `°C` | `return a - 40.0f;` |
| `maf` | `10` | `g/s` | `return (a * 256.0f + b) / 100.0f;` |
| `throttle` | `11` | `%` | `return a / 2.55f;` |
| `run_time` | `1F` | `s` | `return a * 256.0f + b;` |
| `maf_sensor_a` | `66` | `g/s` | `return (a * 256.0f + b) / 32.0f;` |
| `maf_sensor_b` | `66` | `g/s` | `return (c * 256.0f + d) / 32.0f;` |

#### Fuel / Distance

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `distance_with_mil` | `21` | `km` | `return a * 256.0f + b;` |
| `fuel_rail_pressure` | `22` | `kPa` | `return 0.079f * (a * 256.0f + b);` |
| `commanded_egr` | `2C` | `%` | `return a / 2.55f;` |
| `commanded_evap_purge` | `2E` | `%` | `return a / 2.55f;` |
| `fuel_level` | `2F` | `%` | `return a / 2.55f;` |
| `distance_since_cleared` | `31` | `km` | `return a * 256.0f + b;` |
| `evap_vapor_pressure` | `32` | `kPa` | `return ((int16_t)((a << 8) \| b)) / 4000.0f;` |
| `barometric` | `33` | `hPa` | `return a;` |
| `absolute_evap_vapor_pressure` | `53` | `kPa` | `return (a * 256.0f + b) / 200.0f;` |
| `evap_vapor_pressure_wide` | `54` | `kPa` | `return ((int16_t)((a << 8) \| b)) / 1000.0f;` |
| `fuel_rail_pressure_abs` | `59` | `kPa` | `return (a * 256.0f + b) * 10.0f;` |
| `fuel_rate` | `5E` | `L/h` | `return (a * 256.0f + b) / 20.0f;` |
| `ethanol_percent` | `52` | `%` | `return a / 2.55f;` |
| `fuel_injection_timing` | `5D` | `°` | `return (a * 256.0f + b) / 128.0f - 210.0f;` |
| `engine_fuel_rate_alt` | `9D` | `g/s` | `return (a * 256.0f + b) / 50.0f;` |
| `vehicle_fuel_rate` | `9D` | `g/s` | `return (c * 256.0f + d) / 50.0f;` |
| `exhaust_flow_rate` | `9E` | `kg/h` | `return (a * 256.0f + b) / 5.0f;` |
| `fuel_system_a_use_b1` | `9F` | `%` | `return a / 2.55f;` |
| `fuel_system_b_use_b1` | `9F` | `%` | `return b / 2.55f;` |
| `fuel_system_a_use_b2` | `9F` | `%` | `return c / 2.55f;` |
| `fuel_system_b_use_b2` | `9F` | `%` | `return d / 2.55f;` |
| `odometer` | `A6` | `km` | `uint32_t v = ...; return v / 10.0f;` |
| `engine_odometer` | `D3` | `km` | `uint32_t v = ...; return v / 10.0f;` |

#### Temperature

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `catalyst_temp_b1s1` | `3C` | `°C` | `return (a * 256.0f + b) / 10.0f - 40.0f;` |
| `catalyst_temp_b2s1` | `3D` | `°C` | `return (a * 256.0f + b) / 10.0f - 40.0f;` |
| `catalyst_temp_b1s2` | `3E` | `°C` | `return (a * 256.0f + b) / 10.0f - 40.0f;` |
| `catalyst_temp_b2s2` | `3F` | `°C` | `return (a * 256.0f + b) / 10.0f - 40.0f;` |
| `ambient_temp` | `46` | `°C` | `return a - 40.0f;` |
| `oil_temp` | `5C` | `°C` | `return a - 40.0f;` |
| `coolant_temp_sensor_1` | `67` | `°C` | `return a - 40.0f;` |
| `coolant_temp_sensor_2` | `67` | `°C` | `return b - 40.0f;` |
| `intake_temp_sensor_1` | `68` | `°C` | `return a - 40.0f;` |
| `intake_temp_sensor_2` | `68` | `°C` | `return b - 40.0f;` |
| `egr_temp_sensor_a` | `6B` | `°C` | `return a - 40.0f;` |
| `egr_temp_sensor_b` | `6B` | `°C` | `return b - 40.0f;` |
| `egr_temp_sensor_c` | `6B` | `°C` | `return c - 40.0f;` |
| `egr_temp_sensor_d` | `6B` | `°C` | `return d - 40.0f;` |
| `manifold_surface_temp` | `84` | `°C` | `return a - 40.0f;` |

#### Module / Throttle / Pedal

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `battery_voltage` | `42` | `V` | `return (a * 256.0f + b) / 1000.0f;` |
| `absolute_load` | `43` | `%` | `return (a * 256.0f + b) * 100.0f / 255.0f;` |
| `commanded_afr` | `44` | — | `return 2.0f * (a * 256.0f + b) / 65536.0f;` |
| `relative_throttle` | `45` | `%` | `return a / 2.55f;` |
| `absolute_throttle_b` | `47` | `%` | `return a / 2.55f;` |
| `absolute_throttle_c` | `48` | `%` | `return a / 2.55f;` |
| `accel_pedal_d` | `49` | `%` | `return a / 2.55f;` |
| `accel_pedal_e` | `4A` | `%` | `return a / 2.55f;` |
| `throttle_actuator_ctrl` | `4C` | `%` | `return a / 2.55f;` |
| `time_with_mil` | `4D` | `min` | `return a * 256.0f + b;` |
| `time_since_cleared` | `4E` | `min` | `return a * 256.0f + b;` |
| `relative_accel_pedal` | `5A` | `%` | `return a / 2.55f;` |
| `hybrid_battery` | `5B` | `%` | `return a / 2.55f;` |
| `egr_a_cmd` | `69` | `%` | `return a / 2.55f;` |
| `egr_a_act` | `69` | `%` | `return b / 2.55f;` |
| `egr_a_err` | `69` | `%` | `return (c * 100.0f / 128.0f) - 100.0f;` |
| `egr_b_cmd` | `69` | `%` | `return d / 2.55f;` |
| `throttle_actuator_a_cmd` | `6C` | `%` | `return a / 2.55f;` |
| `throttle_a_relative` | `6C` | `%` | `return b / 2.55f;` |
| `throttle_actuator_b_cmd` | `6C` | `%` | `return c / 2.55f;` |
| `throttle_b_relative` | `6C` | `%` | `return d / 2.55f;` |
| `commanded_boost_pressure_a` | `70` | `kPa` | `return (a * 256.0f + b) / 32.0f;` |
| `boost_pressure_sensor_a` | `70` | `kPa` | `return (c * 256.0f + d) / 32.0f;` |
| `commanded_wastegate_a` | `72` | `%` | `return a / 2.55f;` |
| `wastegate_a_actual` | `72` | `%` | `return b / 2.55f;` |
| `commanded_wastegate_b` | `72` | `%` | `return c / 2.55f;` |
| `wastegate_b_actual` | `72` | `%` | `return d / 2.55f;` |
| `total_engine_run_time` | `7F` | `s` | `uint32_t v = ...; return v;` |
| `hev_battery_voltage` | `9A` | `V` | `return (((uint32_t)c << 8) \| d) / 64.0f;` |
| `vehicle_speed_limit` | `AA` | `km/h` | `return a;` |
| `traction_battery_soh` | `B2` | `%` | `return a / 2.55f;` |
| `state_of_certified_energy` | `D2` | `%` | `return b / 2.55f;` |
| `state_of_certified_range` | `D2` | `%` | `return c / 2.55f;` |

#### Torque

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `demand_torque` | `61` | `%` | `return a - 125.0f;` |
| `actual_torque` | `62` | `%` | `return a - 125.0f;` |
| `ref_torque` | `63` | `N·m` | `return a * 256.0f + b;` |
| `engine_friction_torque` | `8E` | `%` | `return a - 125.0f;` |

#### Oxygen Sensors (Mode 01)

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `o2_sensor_b1s1_voltage` | `14` | `V` | `return a / 200.0f;` |
| `o2_sensor_b1s2_voltage` | `15` | `V` | `return a / 200.0f;` |
| `o2_sensor_b2s1_voltage` | `18` | `V` | `return a / 200.0f;` |
| `o2_sensor_b2s2_voltage` | `19` | `V` | `return a / 200.0f;` |

#### GM Specific (Mode 22)

> [!WARNING]
> **Unverified / Platform-Dependent**: Manufacturer-specific (Mode 22) presets are highly dependent on the vehicle's specific ECU/TCM architecture, year, and model. They are community-sourced, unverified, and might not function or return correct data on all vehicle configurations.

| Preset | PID | Unit | Formula | Description |
|--------|-----|------|---------|-------------|
| `gm_ect_volts` | `1149` | `V` | `return a * 0.02f;` | Coolant Temp Sensor Voltage |
| `gm_iat_volts` | `114B` | `V` | `return a * 0.02f;` | Intake Air Temp Sensor Voltage |
| `gm_oil_life_alt` | `1151` | `%` | `return a * 0.392f;` | Engine Oil Life Monitor (Alt) |
| `gm_oil_temp` | `1154` | `°C` | `return a - 40.0f;` | Engine Oil Temperature |
| `gm_fuel_level_volts` | `1155` | `V` | `return a * 0.02f;` | Fuel Level Sensor Voltage |
| `gm_oil_pressure` | `115C` | `psi` | `return (a * 0.65f) - 17.5f;` | Engine Oil Pressure |
| `gm_fuel_trim_cell` | `1160` | — | `return a;` | Fuel Trim Cell |
| `gm_battery_temp` | `1163` | `°C` | `return a - 40.0f;` | Battery Temperature |
| `gm_battery_current` | `1173` | `A` | `return ((int16_t)((a << 8) \| b)) / 10.0f;` | Battery Current |
| `gm_fuel_injector_pw` | `119B` | `ms` | `return (a * 256.0f + b) * 0.001f;` | Fuel Injector Pulse Width |
| `gm_fuel_pump_duty` | `119C` | `%` | `return a * 0.392f;` | Fuel Pump Duty Cycle |
| `gm_oil_life` | `119F` | `%` | `return a / 2.55f;` | Engine Oil Life Monitor |
| `gm_knock_retard` | `11A6` | `°` | `return a * 0.0878906f;` | Knock Retard |
| `gm_prnd_status` | `11B0` | — | `return a;` | GM PRND Status (Gear Position) |
| `gm_fan_duty` | `162B` | `%` | `return a / 2.55f;` | Cooling Fan Duty Cycle |
| `gm_tcc_duty_cycle` | `192A` | `%` | `return a * 0.392f;` | Torque Converter Clutch Duty Cycle |
| `gm_trans_temp` | `1940` | `°C` | `return a - 40.0f;` | Transmission Fluid Temp |
| `gm_tcc_slip_speed` | `1941` | `rpm` | `return a * 256.0f + b;` | Torque Converter Clutch Slip Speed |
| `gm_tcc_slip` | `1991` | `rpm` | `return ((int16_t)((a << 8) \| b)) / 8.0f;` | Torque Converter Clutch Slip |
| `gm_current_gear` | `199A` | — | `return a;` | Current Gear Position |
| `gm_tpms_lf` | `2813` | `psi` | `return a * 0.145f;` | Tire Pressure Left Front |
| `gm_tpms_rf` | `2814` | `psi` | `return a * 0.145f;` | Tire Pressure Right Front |
| `gm_tpms_lr` | `2815` | `psi` | `return a * 0.145f;` | Tire Pressure Left Rear |
| `gm_tpms_rr` | `2816` | `psi` | `return a * 0.145f;` | Tire Pressure Right Rear |

---

## Response Parsing

### Mode 01 (Standard OBD-II)

```
Command sent : "010C\r"
Response     : "41 0C 1A F8 >"
               ──  ──  ── ──
               │   │   data bytes passed to formula as a, b, c, d
               │   └─ PID echo (0x0C)
               └─ response code (0x40 + mode 0x01)
```

The component strips the first **2 bytes** (response code + PID echo) and passes the remaining bytes as `a`, `b`, `c`, `d` to the formula.

### Mode 22 (UDS / GM Extended)

```
Command sent : "221001\r"
Response     : "62 10 01 DD DD DD >"
               ──  ── ──  data bytes
               │   └──┘
               │   PID echo (2 bytes: 0x10, 0x01)
               └─ response code (0x40 + mode 0x22 = 0x62)
```

The component strips the first **3 bytes** (response code + 2-byte PID echo) and passes the remaining bytes as `a`, `b`, `c`, `d`.

> The strip length is determined automatically by `pid` length:
> - 2-char PID → skip 2 header bytes
> - 4-char PID → skip 3 header bytes

---

## Formula Lambda

The `formula` lambda receives up to four `uint8_t` arguments — `a`, `b`, `c`, `d` — corresponding to the first four payload bytes after header stripping. Parameters beyond the actual response length default to `0`. The lambda must return a `float`.

```yaml
formula: "return a;"                                  # single byte
```

```yaml
formula: "return (a * 256.0f + b) / 4.0f;"           # 2-byte big-endian ÷ 4
```

```yaml
formula: |-
  uint32_t raw = ((uint32_t)a << 24)
               | ((uint32_t)b << 16)
               | ((uint32_t)c <<  8)
               |  (uint32_t)d;
  return raw / 10.0f;                                 # 4-byte big-endian ÷ 10
```

When no `formula` is provided the component concatenates all received payload bytes as a big-endian integer.

---

## Common OBD-II PIDs

Mode `"01"` — all PIDs below have a built-in `preset:` shortcut (see [Presets](#presets)).

| PID | Preset | Name | Bytes | Formula | Unit |
|-----|--------|------|-------|---------|------|
| `04` | `engine_load` | Calculated engine load | 1 | `a / 2.55f` | `%` |
| `05` | `coolant_temp` | Engine coolant temperature | 1 | `a - 40` | `°C` |
| `06` | `short_term_fuel_trim_b1` | Short-term fuel trim bank 1 | 1 | `(a*100/128) - 100` | `%` |
| `07` | `long_term_fuel_trim_b1` | Long-term fuel trim bank 1 | 1 | `(a*100/128) - 100` | `%` |
| `08` | `short_term_fuel_trim_b2` | Short-term fuel trim bank 2 | 1 | `(a*100/128) - 100` | `%` |
| `09` | `long_term_fuel_trim_b2` | Long-term fuel trim bank 2 | 1 | `(a*100/128) - 100` | `%` |
| `0A` | `fuel_pressure` | Fuel pressure (gauge) | 1 | `a * 3` | `kPa` |
| `0B` | `intake_pressure` | Intake manifold pressure | 1 | `a` | `kPa` |
| `0C` | `rpm` | Engine speed | 2 | `(256a + b) / 4` | `rpm` |
| `0D` | `speed` | Vehicle speed | 1 | `a` | `km/h` |
| `0E` | `timing_advance` | Timing advance | 1 | `a/2 - 64` | `°` |
| `0F` | `intake_air_temp` | Intake air temperature | 1 | `a - 40` | `°C` |
| `10` | `maf` | MAF air flow rate | 2 | `(256a + b) / 100` | `g/s` |
| `11` | `throttle` | Throttle position | 1 | `a / 2.55` | `%` |
| `14` | `o2_sensor_b1s1_voltage` | O2 Sensor Bank 1 Sensor 1 Voltage | 1 | `a / 200` | `V` |
| `15` | `o2_sensor_b1s2_voltage` | O2 Sensor Bank 1 Sensor 2 Voltage | 1 | `a / 200` | `V` |
| `18` | `o2_sensor_b2s1_voltage` | O2 Sensor Bank 2 Sensor 1 Voltage | 1 | `a / 200` | `V` |
| `19` | `o2_sensor_b2s2_voltage` | O2 Sensor Bank 2 Sensor 2 Voltage | 1 | `a / 200` | `V` |
| `1F` | `run_time` | Run time since engine start | 2 | `256a + b` | `s` |
| `21` | `distance_with_mil` | Distance traveled with MIL on | 2 | `256a + b` | `km` |
| `22` | `fuel_rail_pressure` | Fuel rail pressure (relative) | 2 | `0.079 × (256a + b)` | `kPa` |
| `2C` | `commanded_egr` | Commanded EGR | 1 | `a / 2.55` | `%` |
| `2E` | `commanded_evap_purge` | Commanded EVAP Purge | 1 | `a / 2.55` | `%` |
| `2F` | `fuel_level` | Fuel tank level | 1 | `a / 2.55` | `%` |
| `31` | `distance_since_cleared` | Distance since codes cleared | 2 | `256a + b` | `km` |
| `32` | `evap_vapor_pressure` | Evap Vapor Pressure | 2 | `((int16_t)((a << 8) \| b)) / 4000` | `kPa` |
| `33` | `barometric` | Absolute barometric pressure | 1 | `a` | `hPa` |
| `3C` | `catalyst_temp_b1s1` | Catalyst temperature B1S1 | 2 | `(256a + b)/10 - 40` | `°C` |
| `3D` | `catalyst_temp_b2s1` | Catalyst temperature B2S1 | 2 | `(256a + b)/10 - 40` | `°C` |
| `3E` | `catalyst_temp_b1s2` | Catalyst temperature B1S2 | 2 | `(256a + b)/10 - 40` | `°C` |
| `3F` | `catalyst_temp_b2s2` | Catalyst temperature B2S2 | 2 | `(256a + b)/10 - 40` | `°C` |
| `42` | `battery_voltage` | Control module voltage | 2 | `(256a + b) / 1000` | `V` |
| `43` | `absolute_load` | Absolute load value | 2 | `(256a + b) × 100 / 255` | `%` |
| `44` | `commanded_afr` | Commanded air-fuel ratio | 2 | `2 × (256a + b) / 65536` | ratio |
| `45` | `relative_throttle` | Relative throttle position | 1 | `a / 2.55` | `%` |
| `46` | `ambient_temp` | Ambient air temperature | 1 | `a - 40` | `°C` |
| `47` | `absolute_throttle_b` | Absolute throttle position B | 1 | `a / 2.55` | `%` |
| `48` | `absolute_throttle_c` | Absolute throttle position C | 1 | `a / 2.55` | `%` |
| `49` | `accel_pedal_d` | Accelerator pedal position D | 1 | `a / 2.55` | `%` |
| `4A` | `accel_pedal_e` | Accelerator pedal position E | 1 | `a / 2.55` | `%` |
| `4C` | `throttle_actuator_ctrl` | Throttle actuator control | 1 | `a / 2.55` | `%` |
| `4D` | `time_with_mil` | Time run with MIL on | 2 | `256a + b` | `min` |
| `4E` | `time_since_cleared` | Time since codes cleared | 2 | `256a + b` | `min` |
| `52` | `ethanol_percent` | Ethanol fuel percentage | 1 | `a / 2.55` | `%` |
| `53` | `absolute_evap_vapor_pressure` | Absolute evap vapor pressure | 2 | `(256a + b) / 200` | `kPa` |
| `54` | `evap_vapor_pressure_wide` | Evap vapor pressure wide | 2 | `((int16_t)((a << 8) \| b)) / 1000` | `kPa` |
| `59` | `fuel_rail_pressure_abs` | Fuel rail pressure (absolute) | 2 | `(256a + b) * 10` | `kPa` |
| `5A` | `relative_accel_pedal` | Relative accelerator pedal | 1 | `a / 2.55` | `%` |
| `5B` | `hybrid_battery` | Hybrid battery remaining life | 1 | `a / 2.55` | `%` |
| `5C` | `oil_temp` | Engine oil temperature | 1 | `a - 40` | `°C` |
| `5D` | `fuel_injection_timing` | Fuel injection timing | 2 | `(256a + b)/128 - 210` | `°` |
| `5E` | `fuel_rate` | Engine fuel rate | 2 | `(256a + b) / 20` | `L/h` |
| `61` | `demand_torque` | Driver's demand torque | 1 | `a - 125` | `%` |
| `62` | `actual_torque` | Actual engine torque | 1 | `a - 125` | `%` |
| `63` | `ref_torque` | Engine reference torque | 2 | `256a + b` | `N·m` |
| `66` | `maf_sensor_a` | MAF air flow rate A | 2 | `(256a + b) / 32` | `g/s` |
| `66` | `maf_sensor_b` | MAF air flow rate B | 2 | `(256c + d) / 32` | `g/s` |
| `67` | `coolant_temp_sensor_1` | Coolant temp sensor 1 | 1 | `a - 40` | `°C` |
| `67` | `coolant_temp_sensor_2` | Coolant temp sensor 2 | 1 | `b - 40` | `°C` |
| `68` | `intake_temp_sensor_1` | Intake temp sensor 1 | 1 | `a - 40` | `°C` |
| `68` | `intake_temp_sensor_2` | Intake temp sensor 2 | 1 | `b - 40` | `°C` |
| `69` | `egr_a_cmd` | Commanded EGR A | 1 | `a / 2.55` | `%` |
| `69` | `egr_a_act` | Actual EGR A | 1 | `b / 2.55` | `%` |
| `69` | `egr_a_err` | EGR A error | 1 | `(100c/128) - 100` | `%` |
| `69` | `egr_b_cmd` | Commanded EGR B | 1 | `d / 2.55` | `%` |
| `6B` | `egr_temp_sensor_a` | EGR temp sensor A | 1 | `a - 40` | `°C` |
| `6B` | `egr_temp_sensor_b` | EGR temp sensor B | 1 | `b - 40` | `°C` |
| `6B` | `egr_temp_sensor_c` | EGR temp sensor C | 1 | `c - 40` | `°C` |
| `6B` | `egr_temp_sensor_d` | EGR temp sensor D | 1 | `d - 40` | `°C` |
| `6C` | `throttle_actuator_a_cmd` | Commanded throttle actuator A | 1 | `a / 2.55` | `%` |
| `6C` | `throttle_a_relative` | Relative throttle position A | 1 | `b / 2.55` | `%` |
| `6C` | `throttle_actuator_b_cmd` | Commanded throttle actuator B | 1 | `c / 2.55` | `%` |
| `6C` | `throttle_b_relative` | Relative throttle position B | 1 | `d / 2.55` | `%` |
| `70` | `commanded_boost_pressure_a` | Commanded boost pressure A | 2 | `(256a + b) / 32` | `kPa` |
| `70` | `boost_pressure_sensor_a` | Boost pressure sensor A | 2 | `(256c + d) / 32` | `kPa` |
| `72` | `commanded_wastegate_a` | Commanded wastegate A | 1 | `a / 2.55` | `%` |
| `72` | `wastegate_a_actual` | Actual wastegate A | 1 | `b / 2.55` | `%` |
| `72` | `commanded_wastegate_b` | Commanded wastegate B | 1 | `c / 2.55` | `%` |
| `72` | `wastegate_b_actual` | Actual wastegate B | 1 | `d / 2.55` | `%` |
| `7F` | `total_engine_run_time` | Total engine run time | 4 | `a<<24 \| b<<16 \| c<<8 \| d` | `s` |
| `84` | `manifold_surface_temp` | Manifold surface temperature | 1 | `a - 40` | `°C` |
| `8E` | `engine_friction_torque` | Engine friction torque percent | 1 | `a - 125` | `%` |
| `9A` | `hev_battery_voltage` | Hybrid pack voltage | 4 | `(256c + d) / 64` | `V` |
| `9D` | `engine_fuel_rate_alt` | Engine fuel rate (Alt) | 2 | `(256a + b) / 50` | `g/s` |
| `9D` | `vehicle_fuel_rate` | Vehicle fuel rate | 2 | `(256c + d) / 50` | `g/s` |
| `9E` | `exhaust_flow_rate` | Exhaust flow rate | 2 | `(256a + b) / 5` | `kg/h` |
| `9F` | `fuel_system_a_use_b1` | Fuel system A bank 1 usage | 1 | `a / 2.55` | `%` |
| `9F` | `fuel_system_b_use_b1` | Fuel system B bank 1 usage | 1 | `b / 2.55` | `%` |
| `9F` | `fuel_system_a_use_b2` | Fuel system A bank 2 usage | 1 | `c / 2.55` | `%` |
| `9F` | `fuel_system_b_use_b2` | Fuel system B bank 2 usage | 1 | `d / 2.55` | `%` |
| `A6` | `odometer` | Odometer | 4 | `(a<<24 \| b<<16 \| c<<8 \| d) / 10` | `km` |
| `AA` | `vehicle_speed_limit` | Vehicle speed limit | 1 | `a` | `km/h` |
| `B2` | `traction_battery_soh` | Traction battery state of health | 1 | `a / 2.55` | `%` |
| `D2` | `state_of_certified_energy` | State of certified energy | 1 | `b / 2.55` | `%` |
| `D2` | `state_of_certified_range` | State of certified range | 1 | `c / 2.55` | `%` |
| `D3` | `engine_odometer` | Engine odometer | 4 | `(a<<24 \| b<<16 \| c<<8 \| d) / 10` | `km` |

Full example — standard sensors:

```yaml
esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: obd_client

ble_elm327:
  ble_client_id: obd_client

sensor:
  - platform: ble_elm327
    name: "Engine RPM"
    preset: rpm
    update_interval: 1s

  - platform: ble_elm327
    name: "Vehicle Speed"
    preset: speed
    update_interval: 1s

  - platform: ble_elm327
    name: "Coolant Temperature"
    preset: coolant_temp
    update_interval: 10s

  - platform: ble_elm327
    name: "Fuel Level"
    preset: fuel_level
    update_interval: 30s

  # Demonstration of a custom OBD-II sensor without using a built-in preset
  - platform: ble_elm327
    name: "Intake Air Temperature"
    pid: "0F"
    mode: "01"
    update_interval: 10s
    formula: "return a - 40.0f;"
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement
```

---

## GM Extended PIDs (Chevrolet Colorado / GMC Canyon)

Combines Mode `01` extended PIDs and Mode `22` UDS PIDs.

### PID Reference

| Mode | PID | Name | Formula | Unit |
|------|-----|------|---------|------|
| `01` | `A6` | Odometer | `uint32_t v = ((uint32_t)a<<24)\|((uint32_t)b<<16)\|((uint32_t)c<<8)\|d; return v / 10.0f;` | `km` |
| `22` | `1149` | ECT Sensor Voltage | `return a * 0.02f;` | `V` |
| `22` | `114B` | IAT Sensor Voltage | `return a * 0.02f;` | `V` |
| `22` | `1151` | Engine Oil Life Monitor (Alt) | `return a * 0.392f;` | `%` |
| `22` | `1154` | Engine Oil Temperature | `return a - 40.0f;` | `°C` |
| `22` | `1155` | Fuel Level Sensor Voltage | `return a * 0.02f;` | `V` |
| `22` | `115C` | Engine Oil Pressure | `return (a * 0.65f) - 17.5f;` | `psi` |
| `22` | `1160` | Fuel Trim Cell | `return a;` | — |
| `22` | `1163` | Battery Temperature | `return a - 40.0f;` | `°C` |
| `22` | `1173` | Battery Current | `return ((int16_t)((a << 8) \| b)) / 10.0f;` | `A` |
| `22` | `119B` | Fuel Injector Pulse Width | `return (a * 256.0f + b) * 0.001f;` | `ms` |
| `22` | `119C` | Fuel Pump Duty Cycle | `return a * 0.392f;` | `%` |
| `22` | `119F` | Engine Oil Life Monitor | `return a / 2.55f;` | `%` |
| `22` | `11A6` | Knock Retard | `return a * 0.0878906f;` | `°` |
| `22` | `11B0` | GM PRND Status (Gear Position) | `return a;` | — |
| `22` | `162B` | Cooling Fan Duty Cycle | `return a / 2.55f;` | `%` |
| `22` | `192A` | Torque Converter Clutch Duty Cycle | `return a * 0.392f;` | `%` |
| `22` | `1940` | Transmission Fluid Temp | `return a - 40.0f;` | `°C` |
| `22` | `1941` | Torque Converter Clutch Slip Speed | `return a * 256.0f + b;` | `rpm` |
| `22` | `1991` | Torque Converter Clutch Slip | `return ((int16_t)((a << 8) \| b)) / 8.0f;` | `rpm` |
| `22` | `199A` | Gear Position (raw) | `return a;` | — |
| `22` | `2813` | Tire Pressure Left Front | `return a * 0.145f;` | `psi` |
| `22` | `2814` | Tire Pressure Right Front | `return a * 0.145f;` | `psi` |
| `22` | `2815` | Tire Pressure Left Rear | `return a * 0.145f;` | `psi` |
| `22` | `2816` | Tire Pressure Right Rear | `return a * 0.145f;` | `psi` |

### Full example

```yaml
esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: obd_client

ble_elm327:
  ble_client_id: obd_client
  init_commands:
    - "ATZ"
    - "ATE0"
    - "ATL0"
    - "ATS0"
    - "ATH0"
    - "ATSP6"

sensor:
  - platform: ble_elm327
    name: "Odometer"
    preset: odometer
    update_interval: 30s
    on_value:
      then:
        - lambda: |-
            static float initial = -1.0f;
            if (!isnan(x) && x > 0) {
              if (initial < 0) initial = x;
              id(trip_distance).publish_state(x - initial);
            }

  - platform: template
    id: trip_distance
    name: "Trip Distance"
    unit_of_measurement: "km"
    device_class: distance
    state_class: measurement
    accuracy_decimals: 0

  - platform: ble_elm327
    name: "Gear Position"
    preset: gm_current_gear
    update_interval: 1s

  - platform: ble_elm327
    name: "Engine Oil Life"
    preset: gm_oil_life
    update_interval: 60s

  - platform: ble_elm327
    name: "Transmission Fluid Temperature"
    preset: gm_trans_temp
    update_interval: 10s

  # Demonstration of a custom manufacturer-specific UDS (Mode 22) sensor without preset (1-byte payload)
  - platform: ble_elm327
    name: "Engine Oil Pressure"
    pid: "115C"
    mode: "22"
    update_interval: 5s
    formula: "return (a * 0.65f) - 17.5f;"
    unit_of_measurement: "psi"
    state_class: measurement

  # Demonstration of a custom manufacturer-specific UDS (Mode 22) sensor without preset (2-byte payload)
  - platform: ble_elm327
    name: "Torque Converter Clutch Slip"
    pid: "1991"
    mode: "22"
    update_interval: 2s
    formula: "return ((int16_t)((a << 8) | b)) / 8.0f;"
    unit_of_measurement: "rpm"
    state_class: measurement
```

The odometer `on_value` lambda uses a `static` variable to record the first reading each boot and derive trip distance. For persistence across reboots use an ESPHome `global:` variable instead.

---

## Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|----------|
| Never connects | Wrong MAC address | Double-check with a BLE scanner app |
| Connects but no data | Wrong service / characteristic UUID | Scan the adapter's BLE services and update `service_uuid`, `rx_char_uuid`, `tx_char_uuid` |
| `RX characteristic not found` | UUID mismatch | Some adapters use 128-bit UUIDs — use the full form in config |
| Sensor never updates | PID/mode mismatch or ECU doesn't support it | Enable DEBUG logs and confirm the response byte matches `0x40 + mode`; check `ATDP` to verify protocol |
| Sensors never update after connect | Adapter slow to respond to init commands | Increase `tx_delay` or set `init_commands: []` |
| Data wrong / garbled | Wrong `formula` | Check raw response in DEBUG logs and adjust formula |
| Sensors update too fast / slow | Per-sensor `update_interval` | Tune each sensor's `update_interval` independently |

Enable debug logging:

```yaml
logger:
  level: DEBUG
  logs:
    ble_elm327: DEBUG
    ble_elm327.sensor: DEBUG
```

The DEBUG log shows every BLE write (`>>`) and received response (`<<`):

```
[D][ble_elm327:???]: >> 010C\r
[D][ble_elm327:???]: << 41 0C 1A F8
```

---

## License

MIT License — [@eigger](https://github.com/eigger)
