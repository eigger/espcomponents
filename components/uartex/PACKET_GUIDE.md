# Building UARTEx configurations from packet evidence

Use this guide to turn a device specification or a labelled UART capture into ESPHome entities. It covers framing, entity selection, decoding, command construction and verification. The implementation baseline is espcomponents `e3b1d72`, reviewed on 2026-11-21 with ESPHome 2026.11.0. For an installed device, resolve its external component ref first. Read the Python schema for accepted fields and the corresponding C++ publish/control methods for byte interpretation and units. [Pinned source](https://github.com/eigger/espcomponents/tree/e3b1d72680e30377b1ddc3c4fa3116beb0a968a0/components/uartex) · [Korean companion](PACKET_GUIDE.ko.md)

## Identify the payload before assigning entities

1. Record the model, firmware if known, UART settings, capture point, direction from the ESP's perspective and the physical action or display value. Collect two values for each changing field and a sample from another address or channel.
2. Mark `header | payload | checksum | footer` for each direction. Entity filters, numeric fields, command data and expected ACKs use the payload. The bus-level `on_read` and `on_write` automations see the full frame. A length lambda receives the accumulated frame including its header. State lambdas receive the extracted payload.
3. For constant total sizes use `rx_length`. For an embedded length use `rx_data_length`. Its total equals header size + payload offset of the length field + field width + decoded value + adjust + checksum size + footer size. Check two differently sized samples. A footer, explicit length or checksum provides a frame boundary. Idle time alone cannot delimit packets.
4. Calculate checksums on several frames. For example, `02 01 | 10 03 01 | 17 | 0D 0A` satisfies `add` because the header and payload sum to hex 17. A receiver configured with that header sees payload offset 2 as 01. A matching `state: [0x10, 0x03]` leaves this origin unchanged.
5. Classify reports, requests, echoes and acknowledgments using direction, timing and repeated observations. An ACK can confirm receipt while a later report supplies the final device state. For an unknown TX opcode, deliver the confirmed RX mapping and identify the capture needed to resolve TX. [Parser](parser.cpp) · [Frame dispatch and ACK matching](uartex.cpp) · [Checksum implementation](checksum.cpp)

## Choose an entity from the device's behavior

| Device behavior | Platform | Mapping decisions |
|---|---|---|
| Contact or motion report | `binary_sensor` | Filter by device and opcode with `state`; compare `state_on/off` at the absolute payload offset. |
| One-shot call | `button` | `command_on` runs when pressed. This platform rejects `state` and `command_update`. |
| Relay with feedback | `switch` | Map ON/OFF commands and reports separately. `optimistic` (default `true`, controllable platforms only) publishes the requested state immediately; set `optimistic: false` to wait for feedback. |
| Dimmable light | `light` | RX brightness and TX lambda input use 0–100. For a wire byte 0–255, decode as byte × 100 / 255 and encode as round(x × 255 / 100). |
| Measurement or writable setpoint | `sensor`, `number` | A sensor requires exactly one of `state_number` and `lambda`. For a writable number, also define the input range, step and `command_number`. Verify signedness, byte order and decimal scaling against displayed values. |
| Device status or display text | `text_sensor`, `text` | A text sensor requires a string-returning lambda. Writable text requires `command_text`, which receives `str`. Confirm encoding, terminator and maximum wire length from the device. |
| Enumerated mode | `select` | Declare `options`, decode reports to those exact strings, and map each selectable value to its command. A returned string outside the list does not change the selected option. |
| Heating or cooling | `climate` | Map mode, operating action, current temperature and target separately. For a combined mode/setpoint command, see the thermostat example below. |
| Multi-speed fan | `fan` | `state_speed` and `command_speed` are required. Tune `speed_cnt` to the device. The speed command receives the selected level. Verify power and speed independently. |
| Blind or shutter | `cover` | Positions use 0–1 internally. Endpoints invoke `command_close/open`; intermediate values invoke `command_position`. Translate device percentages both ways. |
| Lock or valve | `lock`, `valve` | Use the specific locked/unlocked or open/closed states. A close-only valve needs only its supported command. The valve schema has no `command_position`. |
| Water heater | `water_heater` | Match the reported operating mode, current and target temperatures, and away state where present. Map each supported write separately. |
| Audio transport and volume | `media_player` | Decode playback states independently of volume. RX volume is 0–1; the TX volume lambda receives 0–100. A percentage byte 50 should publish 0.50 and round-trip to byte 50. |

## Write the smallest configuration that explains the capture

Keep fixed byte lists declarative. Use a mapping for offsets, bit masks, an expected ACK or a preserved state byte. In a state pattern, the default comparison is a prefix at the given offset. With `match: exact`, the payload must end at `offset + data length`. Numeric fields default to one signed big-endian byte; set `signed: false` explicitly for unsigned device fields. Use a lambda for a non-linear lookup or a command computed from the requested value.

For example, consider a hypothetical measurement frame `AA BB | 03 11 22 33 | CE`. The first payload byte counts the next three bytes. Use `rx_header: [0xAA, 0xBB]`, `rx_data_length: {offset: 0, length: 1}` and `rx_checksum: add`. If 11 identifies the measurement and 22 33 is an unsigned big-endian value in tenths, use `state: {offset: 1, data: [0x11]}` and `state_number: {offset: 2, length: 2, signed: false, precision: 1}`. The result should be 875.5. Filtering the identifier leaves both the length byte and the identifier in the coordinate system.

Choose the transmit mechanism from the exchange observed on the bus. An entity's `command_update` polls at its update interval. A command's `ack` matches the start of a received payload after sending. An entity's `state_response` gates when a queued command can be dequeued based on the most recently received payload. A bus-level `rx_reply` queues an outgoing response to a matching incoming request. For a one-off automation use `uartex.write`; supply only the payload and let the bus add TX framing. [Entity queue and state matching](uartex_device.cpp) · [Bus schema and action](__init__.py)

## Worked mappings from repository presets

The byte values below are **test vectors derived from the named presets**. They illustrate what those configurations expect. Check the target device's own samples before reusing an address, opcode, range or ACK.

### CVNet thermostat — two fields in one byte

For heater 1, the report starts with `20 01 4A`. Payload offset 4 holds heat in bit 7 and the target temperature in bits 0–6. Offset 5 holds current temperature. With bytes `98 16` at offsets 4–5, the expected result is heat, target 24 °C, current 22 °C. Changing only 98 to 18 turns heat off while retaining the target. The following entity excerpt uses the numeric mask supported by UARTEx to express the preset's bit extraction. [CVNet preset](../../packages/wallpad/cvnet/cvnet.yaml) · [Climate control](climate/uartex_climate.cpp)

```yaml
state: {data: [0x20, 0x01, 0x4A, 0, 0, 0], mask: [0xFF, 0xFF, 0xFF, 0, 0, 0]}
state_heat: {offset: 4, data: [0x80], mask: [0x80]}
state_off: {offset: 4, data: [0x00], mask: [0x80]}
state_temperature_target: {offset: 4, mask: [0x7F], signed: false}
state_temperature_current: {offset: 5}
```

For a simultaneous request for heat and 25 °C, the preset builds `20 41 01 11 99 00 00 00 00 00 00 00` and expects ACK prefix `20 01 41 91`. During mode control, use the entity's `use_command_target_temperature()` to get the requested temperature, check it against the device range (6–30 °C here), then set bit 7. Also test a temperature-only request and an off request that retains the target.

### CVNet ventilator and gas valve — same bus, different capabilities

The fan report prefix is `20 01 71`. Offset 4 is power and offset 6 is speed. With offset 4 = 01 and offset 6 = 03, expect ON at level 3. Selecting level 2 builds `20 71 01 11 01 01 02 00 00 00 00 00`, with ACK prefix `20 01 71 91`. That ACK shares the report prefix. Test the actual ACK payload against the fan's state rules as well as the command's ACK rule. UARTEx checks ACKs and still dispatches received frames to entities.

The valve reports with prefix `20 01 11`; offset 4 gives open (01) or closed (00). The preset defines a close command and ACK prefix `20 01 11 91`. Report both physical positions and expose the confirmed close operation. Observing the open state provides no TX bytes for an open operation. [CVNet fan and valve](../../packages/wallpad/cvnet/cvnet.yaml) · [Fan control](fan/uartex_fan.cpp) · [Valve schema](valve/__init__.py)

### Whole-room lights — preserving neighboring channels

In the Kocom light fragment the channel byte is at payload offset `7 + light_num` (channels 1–3). A command writes the group. For channel 1, preserve offsets 9 and 10 from the last accepted state. Test a sequence with channel 2 on: turning channel 1 on must leave channel 2 on in the transmitted payload. Also test first boot without a report and two consecutive commands; the second preserving command needs fresh matching feedback. Use `preserve_state` only on a supported entity command with a state filter and matching RX/TX byte positions. [Kocom light fragment](../../packages/wallpad/kocom/fragments/light.yaml) · [State preservation](uartex_device.cpp)

## Decode and encode cross-checks

| Payload or user request | Expected configuration behavior |
|---|---|
| Signed temperature field `FF 9C` | Two-byte signed big-endian with `precision: 1` gives −10.0. A swapped-byte test distinguishes endianness. |
| A blind reports a percentage byte `32` (hex) | `state_position` with `precision: 2` yields 0.50. A request for 0.50 should encode decimal 50; verify open and close separately. |
| A mode report is truncated or has an unknown code | Check `len` before indexing in a lambda. For `select`, return a value absent from `options` to retain the current choice. |
| An ASCII display expects opcode 10 followed by “ABC” | A text command with this layout should emit hex `10 41 42 43`. Check whether the protocol counts a length byte, appends a terminator or pads the field. |
| Pattern matches another channel | Apply a device/opcode filter before extracting values. With a state mask, compare `received_byte & mask` against the configured expected byte. A command mask applies to the received ACK starting at payload offset 0. |

## Deliver and verify

Provide the YAML, the revision used, a byte map with offsets and units, and a short table of input → expected entity state or output bytes. Include a different address, unknown value, short frame and minimum/maximum numeric values where applicable. Reconstruct TX frames including header, checksum and footer and compare with the capture. Label derived test vectors, confirmed observations and unresolved fields individually.

Run `esphome config <board.yaml>` on a complete board configuration and inspect its warnings. Compile custom lambdas with `esphome compile <board.yaml>`. Record configuration validation, compilation and hardware checks separately. The repository has host tests for the parser, masks, commands, replies and preservation; they can be run with `tests/native/uartex_parser/run.sh`. [Validation code](packet_validation.py) · [Host tests](../../tests/native/uartex_parser)
