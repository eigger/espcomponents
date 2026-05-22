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
  id: elm
  mac_address: "AA:BB:CC:DD:EE:FF"

sensor:
  - platform: ble_elm327
    ble_elm327_id: elm
    name: "Engine RPM"
    pid: "0C"
    mode: "01"
    update_interval: 1s
    formula: "return (a * 256.0f + b) / 4.0f;"
    unit_of_measurement: "rpm"

  - platform: ble_elm327
    ble_elm327_id: elm
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
IDLE → (BLE connect) → CONNECTED
     → (service discovery) → sends init_commands[0]
     → INITIALIZING → (response '>' per command) → next command …
     → READY
```

In **READY** state, `loop()` drains the per-device TX queue with `tx_delay` between each write — commands are sent without waiting for a response. When a response arrives it is broadcast to **all** registered devices; each device checks whether the mode + PID bytes match its own configuration and updates its state if they do.

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
    ble_elm327_id: elm
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
    ble_elm327_id: elm
    name: "Engine RPM"
    preset: rpm
    update_interval: 1s

  - platform: ble_elm327
    ble_elm327_id: elm
    name: "Vehicle Speed"
    preset: speed
    update_interval: 1s

  - platform: ble_elm327
    ble_elm327_id: elm
    name: "Coolant Temperature"
    preset: coolant_temp
    update_interval: 10s
```

You can still override any individual field when using a preset:

```yaml
sensor:
  - platform: ble_elm327
    ble_elm327_id: elm
    name: "Fuel Level"
    preset: fuel_level
    update_interval: 60s
    accuracy_decimals: 0   # override the preset default of 1
```

### Available Presets

| Preset | PID | Unit | Formula |
|--------|-----|------|---------|
| `engine_load` | `04` | `%` | `return a / 2.55f;` |
| `coolant_temp` | `05` | `°C` | `return a - 40.0f;` |
| `fuel_pressure` | `0A` | `kPa` | `return a * 3.0f;` |
| `intake_pressure` | `0B` | `kPa` | `return a;` |
| `rpm` | `0C` | `rpm` | `return (a * 256.0f + b) / 4.0f;` |
| `speed` | `0D` | `km/h` | `return a;` |
| `intake_air_temp` | `0F` | `°C` | `return a - 40.0f;` |
| `maf` | `10` | `g/s` | `return (a * 256.0f + b) / 100.0f;` |
| `throttle` | `11` | `%` | `return a / 2.55f;` |
| `run_time` | `1F` | `s` | `return a * 256.0f + b;` |
| `fuel_level` | `2F` | `%` | `return a / 2.55f;` |
| `battery_voltage` | `42` | `V` | `return (a * 256.0f + b) / 1000.0f;` |
| `barometric` | `33` | `hPa` | `return a;` |
| `ambient_temp` | `46` | `°C` | `return a - 40.0f;` |
| `oil_temp` | `5C` | `°C` | `return a - 40.0f;` |

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

Mode `"01"` — 2-char PID, response stripped to data bytes.

| PID | Name | Formula | Unit |
|-----|------|---------|------|
| `04` | Engine load | `return a / 2.55f;` | `%` |
| `05` | Coolant temperature | `return a - 40.0f;` | `°C` |
| `0A` | Fuel pressure | `return a * 3.0f;` | `kPa` |
| `0B` | Intake manifold pressure | `return a;` | `kPa` |
| `0C` | Engine RPM | `return (a * 256.0f + b) / 4.0f;` | `rpm` |
| `0D` | Vehicle speed | `return a;` | `km/h` |
| `0F` | Intake air temperature | `return a - 40.0f;` | `°C` |
| `10` | MAF air flow | `return (a * 256.0f + b) / 100.0f;` | `g/s` |
| `11` | Throttle position | `return a / 2.55f;` | `%` |
| `1F` | Run time since engine start | `return a * 256.0f + b;` | `s` |
| `2F` | Fuel tank level | `return a / 2.55f;` | `%` |
| `33` | Barometric pressure | `return a;` | `kPa` |
| `46` | Ambient air temperature | `return a - 40.0f;` | `°C` |
| `5C` | Engine oil temperature | `return a - 40.0f;` | `°C` |

Full example — standard sensors:

```yaml
esp32_ble_tracker:

ble_elm327:
  id: elm
  mac_address: "AA:BB:CC:DD:EE:FF"

sensor:
  - platform: ble_elm327
    ble_elm327_id: elm
    name: "Engine RPM"
    pid: "0C"
    mode: "01"
    update_interval: 1s
    formula: "return (a * 256.0f + b) / 4.0f;"
    unit_of_measurement: "rpm"
    state_class: measurement

  - platform: ble_elm327
    ble_elm327_id: elm
    name: "Vehicle Speed"
    pid: "0D"
    mode: "01"
    update_interval: 1s
    formula: "return a;"
    unit_of_measurement: "km/h"
    state_class: measurement

  - platform: ble_elm327
    ble_elm327_id: elm
    name: "Coolant Temperature"
    pid: "05"
    mode: "01"
    update_interval: 10s
    formula: "return a - 40.0f;"
    unit_of_measurement: "°C"
    device_class: temperature
    state_class: measurement

  - platform: ble_elm327
    ble_elm327_id: elm
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

## GM Extended PIDs (Mode 22)

Mode `"22"` uses UDS "Read Data By Identifier". The 4-char PID is the 2-byte identifier.

```
Command : "22{pid}\r"   e.g. "221001\r"
Response: "62 10 01 DD DD … >"  (3-byte header stripped automatically)
```

### Chevrolet Colorado / GMC Canyon

| PID | Name | Formula | Unit |
|-----|------|---------|------|
| `1001` | Odometer | `uint32_t v = ((uint32_t)a<<24)\|((uint32_t)b<<16)\|((uint32_t)c<<8)\|d; return v / 10.0f;` | `km` |
| `1005` | Gear position (raw) | `return a;` | — |

#### Gear position mapping example

```yaml
esp32_ble_tracker:

ble_elm327:
  id: elm
  mac_address: "AA:BB:CC:DD:EE:FF"

sensor:
  - platform: ble_elm327
    ble_elm327_id: elm
    name: "Odometer"
    pid: "1001"
    mode: "22"
    update_interval: 30s
    formula: |-
      uint32_t v = ((uint32_t)a << 24)
                 | ((uint32_t)b << 16)
                 | ((uint32_t)c <<  8)
                 |  (uint32_t)d;
      return v / 10.0f;
    unit_of_measurement: "km"
    state_class: total_increasing

  - platform: ble_elm327
    ble_elm327_id: elm
    name: "Gear Position"
    pid: "1005"
    mode: "22"
    update_interval: 1s
    formula: "return a;"
```

Map the raw gear value to a text label in Home Assistant with a `value_template` or using a `template` sensor.

---

## Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|----------|
| Never connects | Wrong MAC address | Double-check with a BLE scanner app |
| Connects but no data | Wrong service / characteristic UUID | Scan the adapter's BLE services and update `service_uuid`, `rx_char_uuid`, `tx_char_uuid` |
| `RX characteristic not found` | UUID mismatch | Some adapters use 128-bit UUIDs — use the full form in config |
| Sensor never updates | PID/mode mismatch or ECU doesn't support it | Enable DEBUG logs and confirm the response byte matches `0x40 + mode`; check `ATDP` to verify protocol |
| Init fails (stays INITIALIZING) | Adapter doesn't respond to `ATZ` | Set `init_commands: []` or use a shorter sequence |
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
