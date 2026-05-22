# BLE ELM327 Component

[![ESPHome](https://img.shields.io/badge/ESPHome-Custom%20Component-blue)](https://esphome.io/)
[![Version](https://img.shields.io/badge/version-1.1.0-green)](https://github.com/eigger/espcomponents)

A custom ESPHome component that connects to a Bluetooth LE ELM327 OBD-II adapter and exposes vehicle data (RPM, speed, temperature, odometer, gear position, etc.) as Home Assistant sensors.

Supports standard OBD-II (Mode 01) and vendor-extended UDS PIDs (Mode 22, e.g. GM/Chevrolet Colorado).

The component manages its own BLE connection internally — no separate `ble_client:` block is needed.

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

ble_elm327:
  mac_address: "AA:BB:CC:DD:EE:FF"

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
ble_elm327:
  id: elm
  mac_address: "AA:BB:CC:DD:EE:FF"
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
| `mac_address` | MAC | **Required** | Bluetooth MAC address of the ELM327 adapter |
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
  mac_address: "AA:BB:CC:DD:EE:FF"
  init_commands: []
```

Custom init sequence example (CAN 500 kbps, headers on):

```yaml
ble_elm327:
  mac_address: "AA:BB:CC:DD:EE:FF"
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

#### Fuel / Distance

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `distance_with_mil` | `21` | `km` | `return a * 256.0f + b;` |
| `fuel_rail_pressure` | `22` | `kPa` | `return 0.079f * (a * 256.0f + b);` |
| `commanded_egr` | `2C` | `%` | `return a / 2.55f;` |
| `fuel_level` | `2F` | `%` | `return a / 2.55f;` |
| `distance_since_cleared` | `31` | `km` | `return a * 256.0f + b;` |
| `barometric` | `33` | `hPa` | `return a;` |
| `fuel_rate` | `5E` | `L/h` | `return (a * 256.0f + b) / 20.0f;` |
| `ethanol_percent` | `52` | `%` | `return a / 2.55f;` |
| `fuel_injection_timing` | `5D` | `°` | `return (a * 256.0f + b) / 128.0f - 210.0f;` |
| `odometer` | `A6` | `km` | `uint32_t v = ...; return v / 10.0f;` |

#### Temperature

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `catalyst_temp_b1s1` | `3C` | `°C` | `return (a * 256.0f + b) / 10.0f - 40.0f;` |
| `catalyst_temp_b2s1` | `3D` | `°C` | `return (a * 256.0f + b) / 10.0f - 40.0f;` |
| `catalyst_temp_b1s2` | `3E` | `°C` | `return (a * 256.0f + b) / 10.0f - 40.0f;` |
| `catalyst_temp_b2s2` | `3F` | `°C` | `return (a * 256.0f + b) / 10.0f - 40.0f;` |
| `ambient_temp` | `46` | `°C` | `return a - 40.0f;` |
| `oil_temp` | `5C` | `°C` | `return a - 40.0f;` |

#### Module / Throttle / Pedal

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `battery_voltage` | `42` | `V` | `return (a * 256.0f + b) / 1000.0f;` |
| `absolute_load` | `43` | `%` | `return (a * 256.0f + b) * 100.0f / 255.0f;` |
| `commanded_afr` | `44` | — | `return 2.0f * (a * 256.0f + b) / 65536.0f;` |
| `relative_throttle` | `45` | `%` | `return a / 2.55f;` |
| `accel_pedal_d` | `49` | `%` | `return a / 2.55f;` |
| `accel_pedal_e` | `4A` | `%` | `return a / 2.55f;` |
| `relative_accel_pedal` | `5A` | `%` | `return a / 2.55f;` |
| `hybrid_battery` | `5B` | `%` | `return a / 2.55f;` |
| `time_with_mil` | `4D` | `min` | `return a * 256.0f + b;` |
| `time_since_cleared` | `4E` | `min` | `return a * 256.0f + b;` |

#### Torque

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `demand_torque` | `61` | `%` | `return a - 125.0f;` |
| `actual_torque` | `62` | `%` | `return a - 125.0f;` |
| `ref_torque` | `63` | `N·m` | `return a * 256.0f + b;` |

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
| `1F` | `run_time` | Run time since engine start | 2 | `256a + b` | `s` |
| `21` | `distance_with_mil` | Distance traveled with MIL on | 2 | `256a + b` | `km` |
| `22` | `fuel_rail_pressure` | Fuel rail pressure (relative) | 2 | `0.079 × (256a + b)` | `kPa` |
| `2C` | `commanded_egr` | Commanded EGR | 1 | `a / 2.55` | `%` |
| `2F` | `fuel_level` | Fuel tank level | 1 | `a / 2.55` | `%` |
| `31` | `distance_since_cleared` | Distance since codes cleared | 2 | `256a + b` | `km` |
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
| `49` | `accel_pedal_d` | Accelerator pedal position D | 1 | `a / 2.55` | `%` |
| `4A` | `accel_pedal_e` | Accelerator pedal position E | 1 | `a / 2.55` | `%` |
| `4D` | `time_with_mil` | Time run with MIL on | 2 | `256a + b` | `min` |
| `4E` | `time_since_cleared` | Time since codes cleared | 2 | `256a + b` | `min` |
| `52` | `ethanol_percent` | Ethanol fuel percentage | 1 | `a / 2.55` | `%` |
| `5A` | `relative_accel_pedal` | Relative accelerator pedal | 1 | `a / 2.55` | `%` |
| `5B` | `hybrid_battery` | Hybrid battery remaining life | 1 | `a / 2.55` | `%` |
| `5C` | `oil_temp` | Engine oil temperature | 1 | `a - 40` | `°C` |
| `5D` | `fuel_injection_timing` | Fuel injection timing | 2 | `(256a + b)/128 - 210` | `°` |
| `5E` | `fuel_rate` | Engine fuel rate | 2 | `(256a + b) / 20` | `L/h` |
| `61` | `demand_torque` | Driver's demand torque | 1 | `a - 125` | `%` |
| `62` | `actual_torque` | Actual engine torque | 1 | `a - 125` | `%` |
| `63` | `ref_torque` | Engine reference torque | 2 | `256a + b` | `N·m` |
| `A6` | `odometer` | Odometer | 4 | `(a<<24\|b<<16\|c<<8\|d) / 10` | `km` |

Full example — standard sensors:

```yaml
esp32_ble_tracker:

ble_elm327:
  mac_address: "AA:BB:CC:DD:EE:FF"

sensor:
  - platform: ble_elm327
    name: "Engine RPM"
    pid: "0C"
    mode: "01"
    update_interval: 1s
    formula: "return (a * 256.0f + b) / 4.0f;"
    unit_of_measurement: "rpm"
    state_class: measurement

  - platform: ble_elm327
    name: "Vehicle Speed"
    pid: "0D"
    mode: "01"
    update_interval: 1s
    formula: "return a;"
    unit_of_measurement: "km/h"
    state_class: measurement

  - platform: ble_elm327
    name: "Coolant Temperature"
    pid: "05"
    mode: "01"
    update_interval: 10s
    formula: "return a - 40.0f;"
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement

  - platform: ble_elm327
    name: "Fuel Level"
    pid: "2F"
    mode: "01"
    update_interval: 30s
    formula: "return a / 2.55f;"
    unit_of_measurement: "%"
    device_class: battery
    state_class: measurement
```

---

## GM Extended PIDs (Chevrolet Colorado / GMC Canyon)

Combines Mode `01` extended PIDs and Mode `22` UDS PIDs.

### PID Reference

| Mode | PID | Name | Formula | Unit |
|------|-----|------|---------|------|
| `01` | `A6` | Odometer | `uint32_t v = ((uint32_t)a<<24)\|((uint32_t)b<<16)\|((uint32_t)c<<8)\|d; return v / 10.0f;` | `km` |
| `22` | `199A` | Gear position (raw) | `return a;` | — |
| `22` | `19F0` | Engine oil life | `return (a * 100.0f) / 255.0f;` | `%` |
| `22` | `1940` | Transmission fluid temp | `return a - 40.0f;` | `°C` |

### Full example

```yaml
esp32_ble_tracker:

ble_elm327:
  mac_address: "AA:BB:CC:DD:EE:FF"
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
    pid: "A6"
    mode: "01"
    update_interval: 30s
    formula: |-
      uint32_t v = ((uint32_t)a << 24)
                 | ((uint32_t)b << 16)
                 | ((uint32_t)c <<  8)
                 |  (uint32_t)d;
      return v / 10.0f;
    unit_of_measurement: "km"
    device_class: distance
    state_class: total_increasing
    accuracy_decimals: 0
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
    pid: "199A"
    mode: "22"
    update_interval: 1s
    formula: "return a;"

  - platform: ble_elm327
    name: "Engine Oil Life"
    pid: "19F0"
    mode: "22"
    update_interval: 60s
    formula: "return (a * 100.0f) / 255.0f;"
    unit_of_measurement: "%"
    state_class: measurement
    accuracy_decimals: 0

  - platform: ble_elm327
    name: "Transmission Fluid Temperature"
    pid: "1940"
    mode: "22"
    update_interval: 10s
    formula: "return a - 40.0f;"
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement
    accuracy_decimals: 0
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
