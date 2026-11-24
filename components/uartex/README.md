# UARTEx Component

[![ESPHome](https://img.shields.io/badge/ESPHome-Custom%20Component-blue)](https://esphome.io/)

A custom ESPHome component that extends UART communication to easily integrate various serial protocols with Home Assistant.

[한국어 README](README.ko.md) · [Packet configuration guide for agents](PACKET_GUIDE.md) · [Wallpad presets](../../packages/wallpad) · [TCP forwarding](../tcp_server).

## Documentation baseline

This README describes UARTEx at espcomponents commit `e3b1d72` (reviewed 2026-11-21). The Quick Start examples passed `esphome config` with ESPHome `2026.11.0` and the component from that commit. [UARTEx source at the documented revision](https://github.com/eigger/espcomponents/tree/e3b1d72680e30377b1ddc3c4fa3116beb0a968a0/components/uartex)

The installation example tracks the moving `latest` tag. To reproduce the examples, replace `latest` in the source URL with the documented commit. For another revision, use its common and platform Python schemas for accepted YAML keys and defaults, and its C++ implementation for packet handling.

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

Choose pins and baud rate for your board and device. This example protocol uses the same payloads for switch commands and state reports. UARTEx adds the configured framing when sending and removes it before matching a received payload.

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
    state_on: [0x01, 0x01]
    state_off: [0x01, 0x00]
    command_on: [0x01, 0x01]
    command_off: [0x01, 0x00]
```

With these settings, the ON frame on the wire is `02 01 | 01 01 | 05 | 0D 0A` (header, payload, additive checksum, footer). The state pattern matches the start of the payload by default.

For a device that reports a channel and a separate status byte, consider the frame `02 01 | 02 03 01 | 09 | 0D 0A`. Here the first two payload bytes identify the report and the third carries its state. The following entity uses the same bus settings:

```yaml
binary_sensor:
  - platform: uartex
    name: "Channel Status"
    state: [0x02, 0x03]
    state_on: {offset: 2, data: [0x01]}
    state_off: {offset: 2, data: [0x00]}
```

Count `offset` from the first payload byte. Matching `state` leaves that origin in place. Use the mapping form when you need an offset, a mask, or an ACK pattern for a command.

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
| `rx_length` | int or lambda | - | Total packet length (1-256) |
| `rx_data_length` | schema | - | Dynamic length parsing (see below) |
| `tx_delay` | time | `50ms` | Minimum quiet time after the latest received byte or transmission (max: 2000ms) |
| `tx_timeout` | time | `50ms` | ACK response timeout (max: 2000ms) |
| `tx_retry_cnt` | int | `3` | Number of retries after the initial transmission on ACK failure (1-10) |
| `tx_command_queue_size` | int | `10` | Deprecated, no effect (accepted for compatibility) |
| `tx_ctrl_pin` | pin | - | RS485 direction control pin |
| `rx_header` | bytes, header schema, or list of headers | - | Receive packet header candidates (see below) |
| `rx_footer` | bytes | - | Receive packet footer |
| `tx_header` | bytes | - | Transmit packet header |
| `tx_footer` | bytes | - | Transmit packet footer |
| `rx_checksum` | enum | - | Receive checksum (1 byte) |
| `tx_checksum` | enum | - | Transmit checksum (1 byte) |
| `rx_checksum2` | enum | - | Receive checksum (multi-byte) |
| `tx_checksum2` | enum | - | Transmit checksum (multi-byte) |
| `rx_priority` | enum | `data` | RX processing mode: `data`, `loop` (see below) |

> **Note**: 
> - Use either `rx_length` or `rx_data_length`, not both.
> - Use either `rx_checksum` or `rx_checksum2`, not both. Same applies to `tx_checksum` and `tx_checksum2`.

ESPHome configuration validation warns about impossible masked header, entity
state, and ACK values, and entity or `rx_reply` definitions that cannot fit the
payload of a fixed-length receive frame. Numeric fields that would be truncated
or missing also produce warnings. These warnings do not prevent configuration
builds. Payload-size checks are skipped when the receive length is dynamic.
Nested automation actions are not checked.

#### Receive headers (`rx_header`)

A single header can be a byte list, an ASCII string, or a mapping with `data`
and an optional bit `mask`. To accept multiple headers, provide a list of
candidates using the same forms:

```yaml
uartex:
  rx_header:
    - [0xA0, 0x01]
    - data: [0xB0, 0x00]
      mask: [0xF0, 0xFF]
    - data: "RX" # ASCII header
```

A flat list is interpreted as a single byte list. For multiple ASCII headers,
use `data` mappings so the list is unambiguously a list of candidates. All
candidates share the configured frame length, footer, and checksum rules.
For overlapping headers, see the priority rules below.

Use an `rx_length` lambda for protocols where a header or opcode determines
the total frame length. The lambda receives `data` and `len` for the bytes
received so far, including the header. Return a positive total length to
set the frame boundary, zero while waiting for more bytes, or a negative
value to discard the current candidate.

For device emulation, `write_immediate_reply(data)` sends a reply from an
`on_read` lambda using the configured TX header, checksum, footer, and
direction pin. The reply leaves any pending command and its ACK timer in place.

#### `rx_priority`: `data` vs `loop`

This controls how received bytes are processed within ESPHome's cooperative main loop.

- **`data`** (default): when bytes start arriving, the component stays in its
  `loop()` until a full frame is parsed or `rx_timeout` elapses, sleeping with
  `delay()` between reads. This gives the tightest RX timing, but it **blocks the
  whole ESPHome main loop** (other components, networking, sensors) for up to
  `rx_timeout` each cycle.
- **`loop`** (**recommended**): non-blocking. Each `loop()` consumes only the
  bytes already available and returns immediately, assembling a frame across
  several iterations. Other components keep running smoothly, so this is the
  better default for most setups — especially with Wi-Fi/API/many entities.

```yaml
uartex:
  rx_priority: loop
```

> **When using `loop`:** incoming bytes wait in the UART hardware buffer between
> iterations, so make sure the `uart:` `rx_buffer_size` is large enough for your
> longest frame plus typical bus traffic to avoid dropped bytes if `loop()` is
> momentarily busy. Both modes require a frame delimiter (`rx_footer`,
> `rx_length`, `rx_data_length`, or a checksum) — pure idle-gap framing is not
> supported.

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
        [0xAA][0xBB][0x03][0x11][0x22][0x33][0xCE]
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
| `xor_add` | High byte: XOR of all bytes; low byte: (sum + XOR) & 0xFF |
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

`version` reports the espcomponents release the firmware was compiled from.
When the built commit is tagged (for example `ref: v261119` or `ref: latest`
in `external_components`), it shows the tag name such as `v261119`. An
untagged commit (e.g. tracking the default branch between releases) shows
the short commit hash, and a local checkout shows `git describe` output such
as `v261119-3-gabc1234`. `unknown` means git was not available.

When receive headers share a prefix, configured order determines priority.
The parser considers the first complete matching header and may treat it as
provisional while an earlier, longer header remains possible. That provisional
header can complete a valid frame; otherwise the earlier header may take
priority as more bytes arrive. Other complete candidates are not tested in
parallel. If the same bytes form a valid shorter frame and the start of a
longer frame, the shorter frame wins at its boundary. Prefer headers without
prefix ambiguity. On a mismatch, the parser retains any suffix that can start
a header. UARTEx requires at least one data byte in a received frame.

`rx_reply` commands are queued ahead of ordinary commands and use the same
`tx_delay` gap after the latest received byte or transmitted frame, including bytes from malformed or incomplete frames. A reply waits
while a command is awaiting its acknowledgment. With `tx_delay: 0ms`, an
eligible reply can be sent in the same loop as the received frame. Continuous
receive traffic can delay replies until the bus has been quiet for `tx_delay`.
The reply queue holds at most 10 waiting commands; when full, newly generated
replies are dropped and the already queued replies keep their order.

#### Event Triggers

In `on_read` and `on_write`, `data` points to `len` bytes for that event.
The bytes remain valid through the automation's delayed and repeated actions.
If a lambda stores `data` for use after that automation finishes, it must copy
the bytes into its own storage. Direct C++ callbacks registered with
`add_on_read_callback` or `add_on_write_callback` may use the pointer only
during the callback and must copy it to retain the data.

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
  mask: [0x7F]       # Optional per-byte AND mask
```

Mask bytes apply in received order starting at `offset`, before binary, BCD,
or ASCII conversion. The mask may contain at most `length` bytes; omitted
mask bytes default to `0xFF`. For signed binary, the masked most significant
byte determines the sign, using the full configured byte width.

Binary and BCD fields use all configured bytes (up to 16). Binary signed
values use two's complement; BCD stops at the first byte with an invalid
decimal nibble. Wide numeric fields are accumulated with double precision and
returned as `float`, so large values are rounded and may become positive or
negative infinity after scaling by precision. ASCII fields retain their
string-to-number conversion behavior.

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

An entity command can preserve other lights' last received state when changing
one light in a whole-room command. In this shortened example, bytes 1, 2 and 3
represent three lights. The command turns on the first light and keeps the
second and third at their last received values:

```yaml
state: [0x10]
command_on:
  data: [0x30, 0xff, 0x00, 0x00]
  ack: [0x40]
  preserve_state: [2, 3]
```

Each offset identifies one byte at the same position in the RX and TX payloads.
Offsets are zero-based and exclude UART headers, checksums, and footers. They
must be unique and fit the command data. The entity must have a `state` filter.
A command waits until an accepted payload contains every preserved byte; other
ready commands and update polls can still transmit. After such a command is
sent, entities whose `state` filters match the source frame wait for another
matching RX payload before sending their next state-preserving command. If no
new state arrives those commands remain queued. Retries reuse the first
transmitted bytes and require another matching state frame afterward.
`preserve_state` is available on entity commands, except `command_update`; it
is not available for `rx_reply` or `uartex.write`. Lambda commands retain their
existing behavior.

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

| Option | Required by schema | Description |
|--------|----------|-------------|
| `state` | No | Base state filter |
| `state_on` | No | ON state match |
| `state_off` | No | OFF state match |
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

| Option | Required by schema | Description |
|--------|----------|-------------|
| `command_on` | No | Press command |

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

| Option | Required by schema | Description |
|--------|----------|-------------|
| `state_on` | No | ON state match |
| `state_off` | No | OFF state match |
| `command_on` | No | ON command |
| `command_off` | No | OFF command |

---

### Light

**Packet Example**:
```
RX ON:       [0x02][0x01][0x01][0x01][0x80][chk][0x0D][0x0A]
RX OFF:      [0x02][0x01][0x01][0x00][0x00][chk][0x0D][0x0A]
             |--header--|------data--------|    |footer|
Offset:                    0     1     2
                                      └─ brightness (0x80 ≈ 50%)
```

```yaml
light:
  - platform: uartex
    name: "Room Light"
    state_on:
      data: [0x01, 0x01]
    state_off:
      data: [0x01, 0x00]
    state_brightness: !lambda |-
      return len > 2 ? data[2] * 100.0f / 255.0f : NAN;
    command_on:
      data: [0x01, 0x01]
    command_off:
      data: [0x01, 0x00]
    command_brightness: !lambda |-
      return {{0x01, 0x02, (uint8_t) lroundf(x * 255.0f / 100.0f)}, {0x01, 0x12}};
```

| Option | Required by schema | Description |
|--------|----------|-------------|
| `state_on` | No | ON state match |
| `state_off` | No | OFF state match |
| `state_brightness` | No | Brightness in percent (0–100) |
| `command_on` | No | ON command |
| `command_off` | No | OFF command |
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

| Option | Required by schema | Description |
|--------|----------|-------------|
| `state` | No | Base state filter |
| `state_number` | One of the two | Numeric value parser |
| `lambda` | One of the two | Custom parser: `float lambda(uint8_t* data, uint16_t len)` |

---

### Text Sensor

```yaml
text_sensor:
  - platform: uartex
    name: "Status"
    state: [0x02, 0x03]
    lambda: |-
      if (len < 3) return "Unknown";
      if (data[2] == 0x01) return "Running";
      if (data[2] == 0x02) return "Idle";
      return "Unknown";
```

| Option | Required by schema | Description |
|--------|----------|-------------|
| `state` | No | Base state filter |
| `lambda` | **Yes** | Text parser: `std::string lambda(uint8_t* data, uint16_t len)` |

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

| Option | Required by schema | Description |
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
      if (len < 3) return "";
      if (data[2] == 0x01) return "Auto";
      if (data[2] == 0x02) return "Manual";
      return data[2] == 0 ? "Off" : "";
    command_select: !lambda |-
      if (str == "Auto") return {{0x02, 0x01}, {}};
      if (str == "Manual") return {{0x02, 0x02}, {}};
      return {{0x02, 0x00}, {}};
```

| Option | Required by schema | Description |
|--------|----------|-------------|
| `options` | **Yes** | Available options |
| `state_select` | No | State parser: `std::string lambda(...)` |
| `command_select` | No | Selection command: `cmd_t lambda(std::string str)` |

---

### Text

```yaml
text:
  - platform: uartex
    name: "Message"
    mode: text
    command_text: !lambda |-
      std::vector<uint8_t> cmd = {0x10};
      for (char c : str) cmd.push_back(c);
      return {cmd, {}};
```

| Option | Required by schema | Description |
|--------|----------|-------------|
| `command_text` | **Yes** | Text command: `cmd_t lambda(std::string str)` |

---

### Climate

If a mode command also encodes the target temperature, use
`id(thermostat).use_command_target_temperature()` for that packet field.
During a combined request it returns the requested temperature. Otherwise it
returns the current target. Once the combined packet is queued, UARTEx updates
the target and skips the separate temperature command. Devices with distinct
mode and temperature packets continue to send both.

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
    id: thermostat
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
      float target = id(thermostat).use_command_target_temperature();
      if (!(target >= 16 && target <= 30)) return {};
      return {{0x03, 0x01, static_cast<uint8_t>(target)}};
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

| Option | Required by schema | Description |
|--------|----------|-------------|
| `state_on` | No | ON state |
| `state_off` | No | OFF state |
| `state_speed` | **Yes** | Speed level (1 through `speed_cnt` while running) |
| `state_preset` | No | Preset parser |
| `command_on` | No | ON command |
| `command_off` | No | OFF command |
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
      precision: 2
    command_open:
      data: [0x05, 0x01]
    command_close:
      data: [0x05, 0x00]
    command_stop:
      data: [0x05, 0x02]
    command_position: !lambda |-
      return {{0x05, 0x03, (uint8_t)(x * 100)}, {}};
```

| Option | Required by schema | Description |
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

| Option | Required by schema | Description |
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

| Option | Required by schema | Description |
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
      precision: 2
    command_play:
      data: [0x08, 0x01]
    command_pause:
      data: [0x08, 0x02]
    command_volume: !lambda |-
      return {{0x08, 0x10, (uint8_t) lroundf(x)}, {}};
```

<details>
<summary><b>All Media Player Options</b></summary>

**States**: `state_none`, `state_idle`, `state_playing`, `state_paused`, `state_announcing`, `state_volume`

**Commands**: `command_play`, `command_pause`, `command_stop`, `command_toggle`, `command_mute`, `command_unmute`, `command_volume`, `command_volume_up`, `command_volume_down`, `command_enqueue`, `command_repeat_one`, `command_repeat_off`, `command_clear_playlist`

</details>

---

## Actions

### uartex.write

Send a payload using the configured TX header, checksum, and footer.

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
| `optimistic` | bool | `false` | For switches, publish the requested state immediately when enabled. Otherwise, publish on matching `state_on` or `state_off` feedback. Enable this for switches that have no state feedback. |
| `state_debounce` | time | `0` | Hold matched state packets and publish only after this time (see below). `0` disables. |
| `update_interval` | time | `60s` | Polling interval |

### State debounce (`state_debounce`)

Some wallpads emit transient state packets (for example a climate flipping `off` -> `heat` for a second or two). `state_debounce` filters such glitches:

- `state` is required. By default the whole matched packet is compared, so a packet whose other bytes keep changing (for example a current temperature in the same packet) is delayed too, and every value in it is applied up to the debounce time later. To avoid that, compare only selected bytes with the `offset` / `mask` form below.
- Matched state packets are held and published only after the debounce time has elapsed since the change started. The latest packet received during that time wins.
- A packet identical to the last applied one (or equal in the masked bytes when `mask` is set) cancels the pending change (the glitch reverted) and is re-published immediately.
- The first matched packet after boot is applied immediately.
- The first state change received right after the entity's own command (within the debounce time of the transmission) is applied immediately, so command results are not delayed even without `optimistic`. Polling (`command_update`) does not trigger this, and a packet identical to the last applied state does not consume it. Only the first state change after the entity's latest transmitted command is immediate: when several commands from one entity go out back-to-back without `ack`, later results are debounced normally. The bypass is armed by commands from the entity's normal command queue (control actions), so a user lambda that periodically calls `enqueue_tx_cmd` would keep it armed.
- `state_response` handling is not delayed.
- Commands from Home Assistant are still reflected immediately through `control` / `optimistic`; only changes observed on the bus are delayed, by up to the debounce time.
- A command with `preserve_state` waits while a change is pending (typically up to the debounce time; longer if bytes in the packet keep changing), because the stored state is not yet settled.
- `0` (default) disables debouncing; the maximum is `60s`.

```yaml
climate:
  - platform: uartex
    name: "Living Room Heater"
    state:
      data: [0x02, 0x03]
    state_debounce: 2s
```

Start with just the time. If other values in the same packet (for example the current temperature) are being delayed, add a `mask` so that only the bytes that matter for the glitch are compared:

```yaml
    state_debounce:
      delay: 2s
      offset: 3              # optional, default 0
      mask: [0xFF, 0x0F]     # only these bits/bytes are compared
```

`offset` and `mask` mean the same as in `state`: the mask is applied to the bytes starting at `offset`. `mask` is required (non-empty) in this form, and `delay` is limited to `60s` as before. `delay: 0s` with a mask is ignored (debounce stays disabled). With a mask:

- A packet whose masked part equals the last applied packet is applied immediately (this also cancels a pending change), so unmasked bytes such as the current temperature update without delay.
- When the masked part changes, the change is held for `delay` as described above, and the whole latest packet is applied after the delay.
- If a byte inside the mask range exists in only one of the two packets they are treated as different; if both packets lack it, it counts as equal.

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
