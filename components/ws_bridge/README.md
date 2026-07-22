# ws_bridge

An ESPHome external component for ESP32 (ESP-IDF only). It connects over a
secure WebSocket directly to Home Assistant's standard `/api/websocket`
endpoint and speaks the protocol of the
[`hass-ws-bridge`](https://github.com/eigger/hass-ws-bridge) custom
integration — declaring entities, pushing state, and receiving commands —
**without needing an MQTT broker**. Combined with a way to reach Home
Assistant securely from outside your LAN (e.g. Nabu Casa remote UI, or your
own reverse proxy with a valid certificate), it can also remove the need for
a VPN just to get a remote device's data into Home Assistant.

> Requires the `ws_bridge` custom component installed on the Home Assistant
> side: https://github.com/eigger/hass-ws-bridge (see its
> [PROTOCOL.md](https://github.com/eigger/hass-ws-bridge/blob/main/docs/PROTOCOL.md)
> for the full wire protocol).

## Installation

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ ws_bridge ]
    refresh: always
```

`ws_bridge` requires the **ESP-IDF** framework (it pulls in ESP-IDF's
`esp_websocket_client` managed component):

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf
```

## Configuration

```yaml
ws_bridge:
  host: 192.168.0.10        # or your Nabu Casa / reverse-proxy hostname
  port: 8123
  ssl: true                 # wss:// (default). Set false only for LAN testing.
  token: !secret ha_token    # Home Assistant long-lived access token
  gateway_id: my_esp         # (default: this device's name)
  name: "My ESP"             # (default: this device's friendly_name)
  keep_last_state_on_disconnect: false

  on_connected:
    - logger.log: "ws_bridge connected"
  on_disconnected:
    - logger.log: "ws_bridge disconnected"

sensor:
  - platform: ws_bridge
    unique_id: temp1
    name: "Temperature"
    device_class: temperature
    unit_of_measurement: "°C"
    state_class: measurement

binary_sensor:
  - platform: ws_bridge
    unique_id: motion1
    name: "Motion"
    device_class: motion

text_sensor:
  - platform: ws_bridge
    unique_id: status1
    name: "Status"

switch:
  - platform: ws_bridge
    unique_id: relay1
    name: "Relay"

number:
  - platform: ws_bridge
    unique_id: setpoint1
    name: "Setpoint"
    min_value: 0
    max_value: 100
    step: 0.5

select:
  - platform: ws_bridge
    unique_id: mode1
    name: "Mode"
    options:
      - "Auto"
      - "Manual"

button:
  - platform: ws_bridge
    unique_id: restart1
    name: "Restart"
```

### `ws_bridge` (hub) options

| Option | Required | Default | Description |
|--------|:--------:|---------|-------------|
| `host` | ✓ | - | Home Assistant address (IP, `.local` hostname, or a remote hostname such as Nabu Casa's) |
| `port` | | 8123 | Port |
| `ssl` | | `true` | Use `wss://`. Only disable for LAN-only testing — the access token is sent in plain text over `ws://` |
| `token` | ✓ | - | Home Assistant long-lived access token |
| `gateway_id` | | device name | Unique client identifier (becomes the HA gateway device) |
| `name` | | device friendly name | Display name for the gateway device |
| `keep_last_state_on_disconnect` | | `false` | If `true`, this gateway's entities keep their last state in HA instead of going `unavailable` when the connection drops (including an ungraceful disconnect) |

### Platform options (all of `sensor`/`binary_sensor`/`text_sensor`/`switch`/`number`/`select`/`button`)

| Option | Required | Description |
|--------|:--------:|-------------|
| `unique_id` | ✓ | Identifier for this entity, unique within the gateway |
| `ws_device_id` | | Groups this entity under a sub-device in HA (e.g. multiple sensors on one physical board) |
| `ws_device_name` | | Display name for that sub-device |

Plus each platform's normal ESPHome options (`name`, `device_class`, `icon`,
`entity_category`, `unit_of_measurement`/`state_class` for `sensor`,
`min_value`/`max_value`/`step` for `number`, `options` for `select`).

## Remote OTA updates (no VPN, no MQTT)

`ws_bridge` itself never carries the firmware binary — it's a small-JSON
protocol, not built for streaming a multi-hundred-KB file. Firmware updates
work over the same outbound-only connection anyway, through ESPHome's
`http_request` OTA/update platforms: the device *pulls* the firmware over a
plain outbound HTTPS request, so no inbound port, VPN, or MQTT broker is
needed.

```yaml
http_request:

ota:
  - platform: http_request
    id: my_ota

update:
  - platform: http_request
    id: my_update
    source: https://your-firmware-host/manifest.json
    update_interval: 6h   # periodically checks for a new version (default 6h)

# Auto-rolls back to the previous firmware if the new one fails to come up
# cleanly. Strongly recommended for any device you can't walk up to.
safe_mode:
```

- `update: platform: http_request` polls `source` (a `manifest.json` in the
  [ESP Web Tools](https://esphome.io/) format below), compares the reported
  `version` against the running firmware, and only flashes when they differ
  — it won't re-flash the same version on every check.
- It shows up in Home Assistant as an `update` entity (current vs. available
  version); install manually from the card, or automate it (e.g. from a
  `ws_bridge` button's `on_press`, or on a schedule) with the
  `update.perform` action.
- **`safe_mode:` matters more than the happy path here** — without it, a
  bad flash on a device you can't physically reach is unrecoverable. It's
  wired to ESP-IDF's app rollback, so a firmware that fails to come up
  cleanly reverts automatically.

`manifest.json` format (ESP Web Tools spec, the same one ESPHome's own
build/dashboard tooling produces):
```json
{
  "name": "My Device",
  "version": "1.0.1",
  "builds": [
    {
      "chipFamily": "ESP32",
      "ota": {
        "path": "firmware.ota.bin",
        "md5": "..."
      }
    }
  ]
}
```

## Triggers

- `on_connected` — the WebSocket connected and Home Assistant accepted the connection
- `on_disconnected` — the connection was lost

## Behavior / Limitations

- **Read-only platforms** (`sensor`, `binary_sensor`, `text_sensor`) push
  their state to Home Assistant automatically whenever it changes.
- **Controllable platforms** (`switch`, `number`, `select`, `button`) receive
  commands from Home Assistant and update their own state optimistically
  (`publish_state`/`this->state`) — hook `on_turn_on`/`lambda:`/etc. in your
  own YAML if you need to drive real hardware from the state.
- On every (re)connect, all declared entities and their current state are
  re-sent, per the protocol's reconnection guidance.
- While connected, an application-level `ping`/`pong` (HA's standard
  websocket_api commands) is sent every 60s. If no `pong` arrives within 15s,
  the connection is forced closed and reopened. This catches a dead
  connection that the transport layer alone wouldn't notice — e.g. Home
  Assistant restarting without a clean WebSocket close, which can otherwise
  leave the device believing it's still connected indefinitely. The interval
  is deliberately low-frequency (worst-case detection is up to ~75s) and the
  timeout generous, so it won't misfire on a slow-but-alive WAN connection
  (e.g. Nabu Casa remote UI, reverse proxy).
- TLS uses ESP-IDF's built-in public CA bundle — this works out of the box
  with Nabu Casa or any certificate from a public CA. A custom CA
  certificate (for self-signed setups) is not supported yet.
- Only one gateway connection per device is supported.
