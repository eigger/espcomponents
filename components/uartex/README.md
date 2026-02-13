# UARTEx Component

[![ESPHome](https://img.shields.io/badge/ESPHome-Custom%20Component-blue)](https://esphome.io/)
[![Version](https://img.shields.io/badge/version-6.3.0-green)](https://github.com/eigger/espcomponents)

A custom ESPHome component that extends UART communication to easily integrate various serial protocols with Home Assistant.

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Configuration Reference](#configuration-reference)
  - [Core Component](#core-component)
  - [Data Schemas](#data-schemas)
- [Platforms](#platforms)
- [Actions](#actions)
- [Troubleshooting](#troubleshooting)

---

## Installation

Add the following to your ESPHome configuration:

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ uartex ]
    refresh: always
```

> **Note**: For time-sensitive sensors (e.g., `log` sensor) that require immediate updates in Home Assistant, disable API batching:
> ```yaml
> api:
>   batch_delay: 0ms
> ```

---

## Quick Start

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO1
  rx_pin: GPIO3
  baud_rate: 9600

uartex:
  uart_id: uart_bus
  rx_header: [0x02, 0x01]
  rx_footer: [0x0D, 0x0A]
  tx_header: [0x02, 0x01]
  tx_footer: [0x0D, 0x0A]
  rx_checksum: add
  tx_checksum: add

switch:
  - platform: uartex
    name: "My Switch"
    state_on:
      data: [0x01, 0x01]
    state_off:
      data: [0x01, 0x00]
    command_on:
      data: [0x01, 0x01]
    command_off:
      data: [0x01, 0x00]
```

---

## Configuration Reference

### Core Component

```yaml
uartex:
  uart_id: uart_bus
  rx_timeout: 10ms
  tx_delay: 50ms
  tx_timeout: 500ms
  tx_retry_cnt: 3
  tx_command_queue_size: 10
  rx_header: [0x02, 0x01]
  rx_footer: [0x0D, 0x0A]
  tx_header: [0x02, 0x01]
  tx_footer: [0x0D, 0x0A]
  rx_checksum: add
  tx_checksum: add
```

#### Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `rx_timeout` | time | `10ms` | Receive timeout (max: 2000ms) |
| `rx_length` | int | - | Fixed packet length (1-256) |
| `rx_data_length` | schema | - | Dynamic length parsing (see below) |
| `tx_delay` | time | `50ms` | Delay between transmissions (max: 2000ms) |
| `tx_timeout` | time | `50ms` | ACK response timeout (max: 2000ms) |
| `tx_retry_cnt` | int | `3` | Retry count on ACK failure (1-10) |
| `tx_command_queue_size` | int | `10` | Command queue size (1-50) |
| `tx_ctrl_pin` | pin | - | RS485 direction control pin |
| `rx_header` | bytes | - | Receive packet header |
| `rx_footer` | bytes | - | Receive packet footer |
| `tx_header` | bytes | - | Transmit packet header |
| `tx_footer` | bytes | - | Transmit packet footer |
| `rx_checksum` | enum | - | Receive checksum (1 byte) |
| `tx_checksum` | enum | - | Transmit checksum (1 byte) |
| `rx_checksum2` | enum | - | Receive checksum (multi-byte) |
| `tx_checksum2` | enum | - | Transmit checksum (multi-byte) |
| `rx_priority` | enum | `data` | Processing priority: `data`, `loop` |

> **Note**: 
> - Use either `rx_length` or `rx_data_length`, not both.
> - Use either `rx_checksum` or `rx_checksum2`, not both. Same applies to `tx_checksum` and `tx_checksum2`.

#### Dynamic Length Parsing (`rx_data_length`)

For protocols where the packet length is embedded within the packet itself:

```yaml
uartex:
  rx_header: [0xAA, 0xBB]
  rx_data_length:
    offset: 0        # Length field position (after header)
    length: 1        # Length field size (1-4 bytes)
    endian: big      # Byte order: big, little
    adjust: 0        # Length adjustment value
  rx_checksum: add
```

**Options**:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `offset` | int | **Required** | Position of length field after header (0-128) |
| `length` | int | `1` | Size of length field in bytes (1-4) |
| `endian` | enum | `big` | Byte order: `big`, `little` |
| `adjust` | int | `0` | Value to add to parsed length (-128 to 128) |

**Packet Structure Example**:
```
Packet: [Header:2][Length:1][Data:N][Checksum:1]
        [0xAA][0xBB][0x03][0x11][0x22][0x33][0xCC]
                     ↑
                     Length field = 3 (data bytes)

Total calculated: header(2) + offset(0) + length_field(1) + data(3) + adjust(0) + checksum(1) = 7 bytes
```

**Use Case**: Protocols where data length varies and is specified in the packet header.

#### Checksum Types

**Single-byte checksum** (`rx_checksum`, `tx_checksum`):

| Type | Description |
|------|-------------|
| `add` | Sum of all bytes |
| `xor` | XOR of all bytes |
| `add_no_header` | Sum excluding header |
| `xor_no_header` | XOR excluding header |
| Lambda | Custom: `uint8_t lambda(uint8_t* data, uint16_t len)` |

**Multi-byte checksum** (`rx_checksum2`, `tx_checksum2`):

| Type | Description |
|------|-------------|
| `xor_add` | XOR + ADD combined (2 bytes) |
| Lambda | Custom: `std::vector<uint8_t> lambda(uint8_t* data, uint16_t len)` |

**Checksum Position**:
- With footer: Checksum is inserted **before the footer**
- Without footer: Checksum is appended **at the end**

```
With footer:    [header][data][checksum][footer]
Without footer: [header][data][checksum]
```


#### Diagnostic Sensors

```yaml
uartex:
  version:
    name: "UARTEx Version"
    disabled: false
  error:
    name: "UARTEx Error"
    disabled: false
  log:
    name: "UARTEx Log"
    disabled: false
    ascii: false  # Set true for ASCII format
```

#### Event Triggers

```yaml
uartex:
  on_read:
    - lambda: |-
        ESP_LOGD("uartex", "Received %d bytes", len);
  on_write:
    - lambda: |-
        ESP_LOGD("uartex", "Sent %d bytes", len);
  on_tx_timeout:
    - logger.log: "Transmission timeout!"
```

---

### Data Schemas

#### State Schema

Defines how to match received data to a specific state.

```yaml
state:
  data: [0x01, 0x02]    # Data to match
  mask: [0xFF, 0xFF]    # Bit mask (optional)
  offset: 0             # Byte offset from header
  inverted: false       # Invert match result
  match: prefix         # Match mode: prefix, exact
```

**Shorthand**: `state: [0x01, 0x02]`

#### State Num Schema

Parses numeric values from received data.

```yaml
state_number:
  offset: 2          # Byte position
  length: 1          # Number of bytes (1-16)
  precision: 0       # Decimal places (0-5)
  signed: true       # Signed integer
  endian: big        # Byte order: big, little
  decode: none       # Decode: none, bcd, ascii
```

#### Understanding Offset

The `offset` value indicates the byte position **after the header**. The header bytes are excluded from offset counting.

**Packet Structure Example**:
```
Full Packet: [0x02][0x01][0xAA][0xBB][0xCC][0xDD][0x0D][0x0A]
             |--header--||---------data---------||--footer--|
Offset:                    0     1     2     3
```

With `rx_header: [0x02, 0x01]` and `rx_footer: [0x0D, 0x0A]`:
- `offset: 0` → byte `0xAA`
- `offset: 1` → byte `0xBB`
- `offset: 2` → byte `0xCC`
- `offset: 3` → byte `0xDD`

> **Note**: Header, checksum, and footer are automatically handled by UARTEx. The `offset` applies only to the payload data portion.


#### State Lambda Types

States also support lambda expressions for custom parsing:

| Lambda Type | Signature | Used By |
|-------------|-----------|---------|
| **Float** | `float lambda(uint8_t* data, uint16_t len)` | `state_number`, `state_brightness`, `state_speed`, `state_temperature_*`, `state_humidity_*`, `state_position`, `state_tilt`, `state_volume` |
| **String** | `std::string lambda(uint8_t* data, uint16_t len)` | `state_select`, `state_preset`, `state_custom_fan`, `state_custom_preset`, `lambda` (text_sensor) |

**Example**:
```cpp
state_number: !lambda |-
  // Parse 2-byte big-endian value with 1 decimal place
  int16_t raw = (data[2] << 8) | data[3];
  return raw / 10.0f;
```

#### Command Schema

Defines data to transmit and optional ACK verification.

```yaml
command:
  data: [0x01, 0x02, 0x01]  # Command bytes
  ack: [0xFF]               # Expected ACK (optional)
  mask: []                  # ACK mask (optional)
```

**Shorthand**: `command_on: [0x01, 0x02, 0x01]`

#### Command Lambda Types

Commands support lambda expressions for dynamic data. The lambda signature varies by command type:

| Lambda Type | Signature | Used By |
|-------------|-----------|---------|
| **Void** | `cmd_t lambda()` | `command_on`, `command_off`, `command_open`, `command_close`, `command_stop`, `command_lock`, `command_unlock`, `command_play`, `command_pause`, `command_mute`, `command_heat`, `command_cool`, etc. |
| **Float** | `cmd_t lambda(float x)` | `command_brightness`, `command_speed`, `command_number`, `command_temperature`, `command_humidity`, `command_volume`, `command_position`, `command_tilt` |
| **String** | `cmd_t lambda(std::string str)` | `command_select`, `command_text`, `command_preset`, `command_custom_fan`, `command_custom_preset` |

**Return format (`cmd_t`)**:
```cpp
// Data only
return {0x01, 0x02, 0x03};

// Data + ACK
return {{0x01, 0x02, 0x03}, {0x01, 0x12}};

// Data + ACK + Mask
return {{0x01, 0x02, 0x03}, {0x01, 0x12}, {0xFF, 0xFF}};
```

---

## Platforms

### Binary Sensor

**Packet Example**:
```
RX ON:  [0x02][0x01][0x02][0x03][0x01][chk][0x0D][0x0A]
RX OFF: [0x02][0x01][0x02][0x03][0x00][chk][0x0D][0x0A]
        |--header--|---------data---------|    |footer|
Offset:              0     1     2
```

```yaml
binary_sensor:
  - platform: uartex
    name: "Motion Sensor"
    state: [0x02, 0x03]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
```

| Option | Required | Description |
|--------|----------|-------------|
| `state` | No | Base state filter |
| `state_on` | **Yes** | ON state match |
| `state_off` | **Yes** | OFF state match |
| `command_update` | No | Status request command |

---

### Button

**Packet Example**:
```
TX:     [0x02][0x01][0x02][0x03][0x01][chk][0x0D][0x0A]
        |--header--|---------data---------|    |footer|
```

```yaml
button:
  - platform: uartex
    name: "Elevator Call"
    command_on:
      data: [0x02, 0x03, 0x01]
```

| Option | Required | Description |
|--------|----------|-------------|
| `command_on` | **Yes** | Press command |

---

### Switch

**Packet Example**:
```
RX ON:  [0x02][0x01][0x01][0x01][chk][0x0D][0x0A]
RX OFF: [0x02][0x01][0x01][0x00][chk][0x0D][0x0A]
TX ON:  [0x02][0x01][0x01][0x01][chk][0x0D][0x0A]
ACK ON: [0x02][0x01][0x01][0x11][chk][0x0D][0x0A]
        |--header--|----data----|    |footer|
Offset:              0     1
```

```yaml
switch:
  - platform: uartex
    name: "Power Switch"
    state_on:
      data: [0x01, 0x01]
    state_off:
      data: [0x01, 0x00]
    command_on:
      data: [0x01, 0x01]
      ack: [0x01, 0x11]
    command_off:
      data: [0x01, 0x00]
      ack: [0x01, 0x10]
```

| Option | Required | Description |
|--------|----------|-------------|
| `state_on` | **Yes** | ON state match |
| `state_off` | **Yes** | OFF state match |
| `command_on` | **Yes** | ON command |
| `command_off` | **Yes** | OFF command |

---

### Light

**Packet Example**:
```
RX ON:       [0x02][0x01][0x01][0x01][0x80][chk][0x0D][0x0A]
RX OFF:      [0x02][0x01][0x01][0x00][0x00][chk][0x0D][0x0A]
             |--header--|------data--------|    |footer|
Offset:                    0     1     2
                                      └─ brightness (0x80 = 128)
```

```yaml
light:
  - platform: uartex
    name: "Room Light"
    state_on:
      data: [0x01, 0x01]
    state_off:
      data: [0x01, 0x00]
    state_brightness:
      offset: 2
      length: 1
    command_on:
      data: [0x01, 0x01]
    command_off:
      data: [0x01, 0x00]
    command_brightness: !lambda |-
      return {{0x01, 0x02, (uint8_t)(x * 255)}, {0x01, 0x12}};
```

| Option | Required | Description |
|--------|----------|-------------|
| `state_on` | **Yes** | ON state match |
| `state_off` | **Yes** | OFF state match |
| `state_brightness` | No | Brightness state (0-1.0) |
| `command_on` | **Yes** | ON command |
| `command_off` | **Yes** | OFF command |
| `command_brightness` | No | Brightness command: `cmd_t lambda(float x)` |

---

### Sensor

**Packet Example**:
```
RX:     [0x02][0x01][0x03][0x01][0x01][0x0A][chk][0x0D][0x0A]
        |--header--|------data--------------|    |footer|
Offset:              0     1     2     3
                                └─────┴─ value: 0x010A = 266 → 26.6 (precision: 1)
```

```yaml
sensor:
  - platform: uartex
    name: "Temperature"
    state: [0x03, 0x01]
    state_number:
      offset: 2
      length: 2
      precision: 1
      signed: true
```

| Option | Required | Description |
|--------|----------|-------------|
| `state` | No | Base state filter |
| `state_number` | No | Numeric value parser |
| `lambda` | No | Custom parser: `float lambda(uint8_t* data, uint16_t len)` |

---

### Text Sensor

```yaml
text_sensor:
  - platform: uartex
    name: "Status"
    state: [0x02, 0x03]
    lambda: |-
      if (data[2] == 0x01) return "Running";
      if (data[2] == 0x02) return "Idle";
      return "Unknown";
```

| Option | Required | Description |
|--------|----------|-------------|
| `state` | No | Base state filter |
| `lambda` | No | Text parser: `std::string lambda(uint8_t* data, uint16_t len)` |

---

### Number

```yaml
number:
  - platform: uartex
    name: "Speed Level"
    min_value: 1
    max_value: 10
    step: 1
    state_number:
      offset: 2
    command_number: !lambda |-
      return {{0x02, 0x01, (uint8_t)x}, {0x02, 0x11}};
```

| Option | Required | Description |
|--------|----------|-------------|
| `state_number` | No | Current value parser |
| `command_number` | No | Set value: `cmd_t lambda(float x)` |

---

### Select

```yaml
select:
  - platform: uartex
    name: "Mode"
    options:
      - "Auto"
      - "Manual"
      - "Off"
    state_select: !lambda |-
      if (data[2] == 0x01) return "Auto";
      if (data[2] == 0x02) return "Manual";
      return "Off";
    command_select: !lambda |-
      if (str == "Auto") return {{0x02, 0x01}, {}};
      if (str == "Manual") return {{0x02, 0x02}, {}};
      return {{0x02, 0x00}, {}};
```

| Option | Required | Description |
|--------|----------|-------------|
| `options` | **Yes** | Available options |
| `state_select` | No | State parser: `std::string lambda(...)` |
| `command_select` | **Yes** | Selection command: `cmd_t lambda(std::string str)` |

---

### Text

```yaml
text:
  - platform: uartex
    name: "Message"
    command_text: !lambda |-
      std::vector<uint8_t> cmd = {0x10};
      for (char c : str) cmd.push_back(c);
      return {cmd, {}};
```

| Option | Required | Description |
|--------|----------|-------------|
| `command_text` | **Yes** | Text command: `cmd_t lambda(std::string str)` |

---

### Climate

**Packet Example**:
```
RX OFF:  [0x02][0x01][0x00][0x18][0x16][chk][0x0D][0x0A]
RX HEAT: [0x02][0x01][0x01][0x18][0x16][chk][0x0D][0x0A]
         |--header--|------data--------|    |footer|
Offset:               0     1     2
                      │     │     └─ current temp (0x16 = 22°C)
                      │     └─ target temp (0x18 = 24°C)
                      └─ mode (0x00=off, 0x01=heat)
```

```yaml
climate:
  - platform: uartex
    name: "Thermostat"
    visual:
      min_temperature: 16
      max_temperature: 30
      temperature_step: 1
    state_off:
      data: [0x00]
    state_heat:
      data: [0x01]
    state_temperature_target:
      offset: 1
    state_temperature_current:
      offset: 2
    command_off:
      data: [0x03, 0x00]
    command_heat: !lambda |-
      return {{0x03, 0x01, (uint8_t)id(thermostat).target_temperature}, {}};
    command_temperature: !lambda |-
      return {{0x03, 0x01, (uint8_t)x}, {}};
```

<details>
<summary><b>All Climate Options</b></summary>

**Mode States**: `state_off`, `state_cool`, `state_heat`, `state_fan_only`, `state_dry`, `state_auto`

**Temperature**: `state_temperature_current`, `state_temperature_target`, `command_temperature`

**Humidity**: `state_humidity_current`, `state_humidity_target`, `command_humidity`

**Fan Modes**: `state_fan_auto`, `state_fan_low`, `state_fan_medium`, `state_fan_high`, `state_fan_quiet`

**Swing Modes**: `state_swing_off`, `state_swing_both`, `state_swing_vertical`, `state_swing_horizontal`

**Presets**: `state_preset_home`, `state_preset_away`, `state_preset_eco`, `state_preset_sleep`, etc.

**Custom Modes**: `custom_fan_mode`, `custom_preset`, `state_custom_fan`, `command_custom_fan`

</details>

---

### Fan

**Packet Example**:
```
RX ON:  [0x02][0x01][0x01][0x03][chk][0x0D][0x0A]
RX OFF: [0x02][0x01][0x00][0x00][chk][0x0D][0x0A]
        |--header--|----data----|    |footer|
Offset:              0     1
                     │     └─ speed (0x03 = level 3)
                     └─ state (0x01=on, 0x00=off)
```

```yaml
fan:
  - platform: uartex
    name: "Ventilator"
    state_on:
      data: [0x01]
    state_off:
      data: [0x00]
    state_speed:
      offset: 1
    command_on:
      data: [0x04, 0x01]
    command_off:
      data: [0x04, 0x00]
    command_speed: !lambda |-
      return {{0x04, 0x01, (uint8_t)x}, {}};
```

| Option | Required | Description |
|--------|----------|-------------|
| `state_on` | **Yes** | ON state |
| `state_off` | **Yes** | OFF state |
| `state_speed` | No | Speed level (0-100) |
| `state_preset` | No | Preset parser |
| `command_on` | **Yes** | ON command |
| `command_off` | **Yes** | OFF command |
| `command_speed` | No | Speed command: `cmd_t lambda(float x)` |
| `preset_modes` | No | Available presets list |

---

### Cover

**Packet Example**:
```
RX OPEN:   [0x02][0x01][0x01][0x64][chk][0x0D][0x0A]
RX CLOSED: [0x02][0x01][0x00][0x00][chk][0x0D][0x0A]
           |--header--|----data----|    |footer|
Offset:                 0     1
                        │     └─ position (0x64 = 100%)
                        └─ state (0x01=open, 0x00=closed)
```

```yaml
cover:
  - platform: uartex
    name: "Blinds"
    state_open:
      data: [0x01]
    state_closed:
      data: [0x00]
    state_position:
      offset: 1
    command_open:
      data: [0x05, 0x01]
    command_close:
      data: [0x05, 0x00]
    command_stop:
      data: [0x05, 0x02]
    command_position: !lambda |-
      return {{0x05, 0x03, (uint8_t)(x * 100)}, {}};
```

| Option | Required | Description |
|--------|----------|-------------|
| `state_open` | No | Open state |
| `state_closed` | No | Closed state |
| `state_position` | No | Position (0.0-1.0) |
| `state_tilt` | No | Tilt angle (0.0-1.0) |
| `command_open` | No | Open command |
| `command_close` | No | Close command |
| `command_stop` | No | Stop command |
| `command_position` | No | Position command |
| `command_tilt` | No | Tilt command |

---

### Lock

**Packet Example**:
```
RX LOCKED:   [0x02][0x01][0x01][chk][0x0D][0x0A]
RX UNLOCKED: [0x02][0x01][0x00][chk][0x0D][0x0A]
             |--header--|-data|    |footer|
Offset:                   0
```

```yaml
lock:
  - platform: uartex
    name: "Door Lock"
    state_locked:
      data: [0x01]
    state_unlocked:
      data: [0x00]
    command_lock:
      data: [0x06, 0x01]
    command_unlock:
      data: [0x06, 0x00]
```

| Option | Required | Description |
|--------|----------|-------------|
| `state_locked` | No | Locked state |
| `state_unlocked` | No | Unlocked state |
| `state_locking` | No | Locking in progress |
| `state_unlocking` | No | Unlocking in progress |
| `state_jammed` | No | Jammed state |
| `command_lock` | No | Lock command |
| `command_unlock` | No | Unlock command |

---

### Valve

```yaml
valve:
  - platform: uartex
    name: "Water Valve"
    state_open:
      data: [0x01]
    state_closed:
      data: [0x00]
    command_open:
      data: [0x07, 0x01]
    command_close:
      data: [0x07, 0x00]
```

| Option | Required | Description |
|--------|----------|-------------|
| `state_open` | No | Open state |
| `state_closed` | No | Closed state |
| `state_position` | No | Position (0.0-1.0) |
| `command_open` | No | Open command |
| `command_close` | No | Close command |
| `command_stop` | No | Stop command |

---

### Water Heater

**Packet Example**:
```
RX ECO:  [0x02][0x01][0x01][0x28][0x32][chk][0x0D][0x0A]
RX GAS:  [0x02][0x01][0x06][0x28][0x32][chk][0x0D][0x0A]
         |--header--|------data--------|    |footer|
Offset:               0     1     2
                      │     │     └─ current temp (0x32 = 50°C)
                      │     └─ target temp (0x28 = 40°C)
                      └─ mode (0x01=eco, 0x06=gas)
```

```yaml
water_heater:
  - platform: uartex
    name: "Water Heater"
    visual:
      min_temperature: 30
      max_temperature: 75
      target_temperature_step: 1
    state_off:
      data: [0x00]
    state_eco:
      data: [0x01]
    state_electric:
      data: [0x02]
    state_performance:
      data: [0x03]
    state_high_demand:
      data: [0x04]
    state_heat_pump:
      data: [0x05]
    state_gas:
      data: [0x06]
    state_temperature_target:
      offset: 1
    state_temperature_current:
      offset: 2
    command_off:
      data: [0x09, 0x00]
    command_eco:
      data: [0x09, 0x01]
    command_electric:
      data: [0x09, 0x02]
    command_gas:
      data: [0x09, 0x06]
    command_temperature: !lambda |-
      return {{0x09, 0x10, (uint8_t)x}, {}};
```

<details>
<summary><b>All Water Heater Options</b></summary>

**Mode States**: `state_off`, `state_eco`, `state_electric`, `state_performance`, `state_high_demand`, `state_heat_pump`, `state_gas`

**Temperature**: `state_temperature_current`, `state_temperature_target`, `command_temperature`

**Away Mode**: `state_away_on`, `state_away_off`, `command_away_on`, `command_away_off`

**Commands**: `command_off`, `command_on`, `command_eco`, `command_electric`, `command_performance`, `command_high_demand`, `command_heat_pump`, `command_gas`

</details>

---

### Media Player

```yaml
media_player:
  - platform: uartex
    name: "Audio Player"
    state_playing:
      data: [0x01]
    state_paused:
      data: [0x02]
    state_idle:
      data: [0x00]
    state_volume:
      offset: 1
    command_play:
      data: [0x08, 0x01]
    command_pause:
      data: [0x08, 0x02]
    command_volume: !lambda |-
      return {{0x08, 0x10, (uint8_t)(x * 100)}, {}};
```

<details>
<summary><b>All Media Player Options</b></summary>

**States**: `state_none`, `state_idle`, `state_playing`, `state_paused`, `state_announcing`, `state_volume`

**Commands**: `command_play`, `command_pause`, `command_stop`, `command_toggle`, `command_mute`, `command_unmute`, `command_volume`, `command_volume_up`, `command_volume_down`, `command_enqueue`, `command_repeat_one`, `command_repeat_off`, `command_clear_playlist`

</details>

---

## Actions

### uartex.write

Send raw data through UART.

```yaml
on_...:
  - uartex.write:
      data: [0x02, 0x01, 0x00]
      ack: [0x02, 0x11]
```

With lambda:

```yaml
on_...:
  - uartex.write:
      data: !lambda |-
        return {0x02, 0x01, (uint8_t)id(sensor1).state};
```

---

## Common Device Options

These options are available for all UARTEx platforms:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `state` | state | - | Base state filter |
| `state_response` | state | - | Response state match |
| `command_update` | command | - | Polling command |
| `optimistic` | bool | `false` | Optimistic state updates |
| `update_interval` | time | `60s` | Polling interval |

---

## Lambda Examples

### Command with ACK

```cpp
// Simple command
return {0x01, 0x02, 0x03};

// Command with ACK
return {{0x01, 0x02, 0x03}, {0x01, 0x12}};

// Command with ACK and mask
return {{0x01, 0x02, 0x03}, {0x01, 0x12}, {0xFF, 0xFF}};
```

### Dynamic Command

```cpp
command_on: !lambda |-
  uint8_t value = id(some_sensor).state;
  return {{0x01, value}, {0x01}};
```

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No communication | Check UART pins, baud rate, and wiring |
| ACK timeout | Increase `tx_timeout`, verify ACK pattern |
| State not updating | Check `state` filter and `offset` values |
| Checksum errors | Verify checksum type matches device protocol |

Enable logging for debugging:

```yaml
logger:
  level: DEBUG

uartex:
  log:
    disabled: false
```

---

## License

MIT License - [@eigger](https://github.com/eigger)
