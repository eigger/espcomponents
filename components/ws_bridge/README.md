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

> Requires the `ws_bridge` custom component **1.2.0+** on the Home Assistant
> side: https://github.com/eigger/hass-ws-bridge (see its
> [PROTOCOL.md](https://github.com/eigger/hass-ws-bridge/blob/main/docs/PROTOCOL.md)
> for the full wire protocol). The `update` platform needs **ESPHome 2025.7+**
> (`UpdateEntity::update_info` / `state` as public refs).

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
  sync_entities: false      # true = remove HA entities this device no longer declares
  ping_interval: 60s
  pong_timeout: 15s
  reconnect_timeout: 30s
  reannounce_interval: 60s

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

# Firmware update entity in HA — wraps ESPHome's http_request update.
# See "Remote OTA updates" below.
http_request:
ota:
  - platform: http_request
update:
  - platform: http_request
    id: ota_update
    source: https://your-firmware-host/manifest.json
  - platform: ws_bridge
    unique_id: firmware
    name: "Firmware"
    update_id: ota_update
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
| `sync_entities` | | `false` | If `true`, sends `ws_bridge/sync` with every declared `unique_id` right after connecting, so HA removes entities this gateway no longer provides. Entities still declared keep their `entity_id`, history and long-term statistics — nothing is wiped and recreated. See [Removing stale entities](#removing-stale-entities) |
| `ping_interval` | | `60s` | How often to send an app-level `ping` once connected, to detect a peer that dropped without a clean WebSocket close |
| `pong_timeout` | | `15s` | How long to wait for a `pong` reply before assuming the connection is dead and forcing a reconnect |
| `reconnect_timeout` | | `30s` | Cap for the reconnect backoff: while disconnected, we retry starting at 2s and doubling on each failure up to this value (matches the companion hass-ble-android client), rather than waiting on `esp_websocket_client`'s own auto-reconnect indefinitely |
| `reannounce_interval` | | `60s` | How often to resend `ws_bridge/connect` + all entity/state declarations while nominally connected. Guards against the HA-side integration losing track of this gateway (e.g. its config entry reloaded) while the raw connection and ping/pong stay healthy — that scenario is otherwise invisible, since HA answers pings regardless of our integration's state. If a re-announce goes unanswered, forces a full reconnect rather than repeating the same no-op |
| `trackers` | | - | List of GPS `device_tracker` entities — see [GPS device trackers](#gps-device-trackers) below |
| `entities` | | - | Existing ESPHome entities to expose without a parallel `platform: ws_bridge` entity — see [Exposing existing entities](#exposing-existing-entities) |

### Platform options (all of `sensor`/`binary_sensor`/`text_sensor`/`switch`/`number`/`select`/`button`/`update`)

| Option | Required | Description |
|--------|:--------:|-------------|
| `unique_id` | ✓ | Identifier for this entity, unique within the gateway. **Changing it creates a new HA entity** (the old one is left behind; `sync_entities: true` then deletes it) |
| `ws_device_id` | | Groups this entity under a sub-device in HA (e.g. multiple sensors on one physical board) |
| `ws_device_name` | | Display name for that sub-device |
| `sensor_id` / `binary_sensor_id` / `text_sensor_id` / `switch_id` / `number_id` / `select_id` / `button_id` | | ID of an existing ESPHome entity to wrap — see [Exposing existing entities](#exposing-existing-entities) |
| `update_id` | ✓ (`update` only) | ID of the ESPHome `update:` entity to wrap — typically `platform: http_request` |
| `name` | (`update`) | HA-facing name. If omitted, uses the wrapped entity's name when that entity has its own `name:`; otherwise the `unique_id` |

Plus each platform's normal ESPHome options (`name`, `device_class`, `icon`,
`entity_category`, `unit_of_measurement`/`state_class` for `sensor`,
`min_value`/`max_value`/`step` for `number`, `options` for `select`). When
wrapping, those inherit from the source unless also set here —
`min_value`/`max_value` (and `options` on `select`) are required only when
*not* wrapping. An id-only `http_request` update is `internal` and named
after its id — set `name:` on the `ws_bridge` wrapper so Home Assistant does
not show `"ota_update"`.

## Exposing existing entities

Two ways to put an already-declared ESPHome entity onto Home Assistant.
Pick **one** per source — using both (`sensor_id:` *and* `entities:`)
exposes it twice. `dump_config()` warns at boot if that happens, or if two
devices share a `unique_id`.

### `platform: ws_bridge` wrapping (`sensor_id:` / `switch_id:` / …)

Creates a parallel ws_bridge entity that mirrors (and, for writable
domains, *drives*) the source. Use this when you want a distinct
`unique_id`, or to override `name:` / `device_class:` / `icon:` / etc.

```yaml
sensor:
  - platform: uptime
    id: uptime_sensor
  - platform: ws_bridge
    unique_id: uptime
    sensor_id: uptime_sensor
    # device_class / unit_of_measurement / state_class / accuracy_decimals
    # inherit from uptime_sensor unless also set here
    name: "Uptime"                 # ESPHome requires id: or name: (unique_id is not enough)

switch:
  - platform: gpio
    pin: GPIO12
    id: relay
  - platform: ws_bridge
    unique_id: relay_ha
    name: "Relay"
    switch_id: relay               # HA on/off is applied to `relay`, not this entity

number:
  - platform: template
    id: setpoint
    optimistic: true
    min_value: 0
    max_value: 10
    step: 1
  - platform: ws_bridge
    unique_id: setpoint_ha
    name: "Setpoint"
    number_id: setpoint            # min_value / max_value / step inherited — omit them
```

- **Metadata** (`device_class`, `icon`, `entity_category`, sensor unit /
  state class / accuracy, number range, select options) comes from the
  source. Anything written on the ws_bridge platform still wins.
- **Commands** on `switch` / `number` / `select` / `button` / `update` are
  applied to the wrapped entity, not to the wrapper. On-device automations
  should keep targeting the source (`switch.turn_on: relay`).
- ESPHome still requires `id:` or `name:` on the wrapper YAML (`unique_id`
  alone is not enough).
- `unique_id` is still required. Changing it (or first adding wrapping
  under a new id) is a **new HA entity** — history stays on the old
  `entity_id`.
- `update:` wrapping (`update_id:`) is required, not optional — that
  platform has no state of its own. See [Remote OTA updates](#remote-ota-updates-no-vpn-no-mqtt).

### Hub `entities:` list

Exposes the source without a parallel `platform: ws_bridge` entity. The
domain is taken from `source_id:` (no `platform:` key). Prefer this when
you just want the entity in HA as-is.

```yaml
ws_bridge:
  host: !secret ha_address
  token: !secret ha_token
  entities:
    - source_id: uptime_sensor          # unique_id defaults to "uptime_sensor"
    - source_id: relay
      unique_id: relay_ha               # optional override
      name: "Living Room Relay"         # optional; else the source's own name
      ws_device_id: board_1
      ws_device_name: "Sensor Hub"
```

| Option | Required | Description |
|--------|:--------:|-------------|
| `source_id` | ✓ | ESPHome `id:` of an existing sensor / binary_sensor / text_sensor / switch / number / select / button / update |
| `unique_id` | | HA identifier. **Defaults to the source's YAML `id:`** (stable across `name:` edits, unlike an object_id) |
| `name` | | HA-facing name. Defaults to the source's own `name:`; an id-only source (no `name:`) falls back to `unique_id` rather than leaking the ESPHome id |
| `ws_device_id` / `ws_device_name` | | Same as every other platform |

There is no per-field metadata override here — `device_class` and friends
come entirely from the source. Use platform wrapping if you need to change
them.

**`id:` changes replace the HA entity.** When `unique_id` is omitted it
tracks the source's YAML `id:`. Rename `id: uptime_sensor` → `id: uptime`
and HA sees a brand-new entity; the old one is orphaned. Set `unique_id:`
explicitly if you want to rename the ESPHome id without churning HA.
The same applies to any platform entity whose `unique_id` you edit.

### `sync_entities` interaction

Both wrapping and `entities:` register as normal ws_bridge devices, so
they are included in the `ws_bridge/sync` list when
`sync_entities: true`. Switching a source from wrapping to `entities:`
(or the other way) under a *different* `unique_id` will therefore delete
the old HA entity on the next connect — including its `entity_id` and
history attachment. Keep the `unique_id` stable across that migration, or
leave `sync_entities` off and remove the leftover by hand.

## Remote OTA updates (no VPN, no MQTT)

`ws_bridge` itself never carries the firmware binary — it's a small-JSON
protocol, not built for streaming a multi-hundred-KB file. Firmware updates
work over the same outbound-only connection anyway, through ESPHome's
`http_request` OTA/update platforms: the device *pulls* the firmware over a
plain outbound HTTPS request, so no inbound port, VPN, or MQTT broker is
needed. `update: platform: ws_bridge` then exposes that on-device updater to
Home Assistant as a real `update` entity (Install button, current vs.
available version), which native `http_request` update cannot do on its own
when the device is not using ESPHome's native API. This requires
`hass-ws-bridge` **1.2.0+** (the `update` platform) and **ESPHome 2025.7+**.

The [ESPHome OTA Publisher](https://github.com/eigger/hassio-apps/tree/master/esphome_ota)
add-on is the intended firmware host: it publishes `firmware.ota.bin` + a
manifest under HA's `/local/` (reachable over LAN and a remote tunnel). It
generates two packages — pick one.

**Force-install button** (`flash_button.yaml`, the add-on's default): no
version tracking, just a button that pulls the `.ota.bin` and checks MD5.
Wrap `ota_flash_button` so it appears in Home Assistant:

```yaml
substitutions:
  ota_device: livingroom          # must match the published node name

packages:
  ota: !include ota_server/flash_button.yaml

button:
  - platform: ws_bridge
    unique_id: ota_update_button
    name: "Update"
    icon: mdi:update
    button_id: ota_flash_button   # id from flash_button.yaml

# Auto-rolls back to the previous firmware if the new one fails to come up
# cleanly. Strongly recommended for any device you can't walk up to.
safe_mode:
```

**Update entity** (`update.yaml`): HA gets a real Install card with current
vs. available version. Needs `esphome.project.version` bumped to offer an
update:

```yaml
substitutions:
  ota_device: livingroom

packages:
  ota: !include ota_server/update.yaml

esphome:
  project:
    name: "you.something"
    version: "1.0.0"

update:
  - platform: ws_bridge
    unique_id: firmware
    name: "Firmware"
    update_id: ota_update         # id from ota_server/update.yaml

safe_mode:
```

Without the add-on, the same pieces work against any host that serves an
ESP Web Tools `manifest.json`:

```yaml
http_request:

ota:
  - platform: http_request
    id: my_ota

update:
  - platform: http_request
    id: ota_update
    source: https://your-firmware-host/manifest.json
    update_interval: 6h   # periodically checks for a new version (default 6h)
  - platform: ws_bridge
    unique_id: firmware
    name: "Firmware"
    update_id: ota_update

safe_mode:
```

- `update: platform: http_request` polls `source` (a `manifest.json` in the
  [ESP Web Tools](https://esphome.io/) format below), compares the reported
  `version` against the running firmware, and only flashes when they differ
  — it won't re-flash the same version on every check. **Without an
  `esphome.project` `version`** the device reports the ESPHome release
  string, so an update only appears when you upgrade ESPHome itself.
- `update: platform: ws_bridge` is what Home Assistant actually sees. Install
  from the update card, or trigger a check with `homeassistant.update_entity`.
  On-device automations still use the wrapped entity (`update.perform:
  ota_update`, `on_update_available`).
- **`safe_mode:` matters more than the happy path here** — without it, a
  bad flash on a device you can't physically reach is unrecoverable. It's
  wired to ESP-IDF's app rollback, so a firmware that fails to come up
  cleanly reverts automatically.

`manifest.json` format (ESP Web Tools spec, the same one ESPHome's own
build/dashboard tooling and the OTA Publisher add-on produce):
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
- `on_declare` — entity declarations are being (re)sent: on connect **and** on
  every periodic re-announce. Use this for hand-built declarations (see below),
  not `on_connected`.

## GPS device trackers

Home Assistant has a `device_tracker` entity type for GPS location, but
ESPHome has no `device_tracker:` domain to hang a `platform: ws_bridge` off —
so these are configured inline under the hub instead:

```yaml
ws_bridge:
  host: !secret ha_address
  token: !secret ha_token

  trackers:
    - unique_id: car_location
      name: "Car Location"
      icon: mdi:car
      latitude: !lambda return id(my_gps).latitude;
      longitude: !lambda return id(my_gps).longitude;
      gps_accuracy: 8          # meters; optional
      update_interval: 30s     # default 60s
```

| Option | Required | Description |
|--------|:--------:|-------------|
| `unique_id` / `name` | ✓ | Same as every other platform |
| `latitude` / `longitude` | ✓ | Float or `!lambda`. Return `NAN` when there's no fix yet — Home Assistant then shows the tracker as unavailable rather than pinning it at 0,0 |
| `gps_accuracy` | | Float or `!lambda`, meters. Home Assistant uses it when deciding zone membership |
| `icon` | | Same as every other platform |
| `update_interval` | | Default `60s` |
| `ws_device_id` / `ws_device_name` | | Same as every other platform |

There's no `battery_level` option — Home Assistant has deprecated it on
`device_tracker` in favor of a separate entity; declare a normal `sensor` with
`device_class: battery` instead.

## Declaring other entity types ESPHome has no domain for

`trackers:` above is really just a thin wrapper around two building blocks
available to any lambda, for whatever other Home Assistant entity type
ESPHome itself doesn't have a domain for:

```yaml
ws_bridge:
  id: my_ws_bridge
  host: !secret ha_address
  token: !secret ha_token

  on_declare:
    - lambda: |-
        id(my_ws_bridge)->send_entity_declare(
            "some_id", "some_platform", "Some Name", "", "", nullptr);

interval:
  - interval: 30s
    then:
      - lambda: |-
          id(my_ws_bridge)->send_state_float("some_id", 42.0f);
```

- **Declare from `on_declare:`, not `on_connected:`.** Declarations are re-sent
  both on connect and on each periodic re-announce; `on_connected` only covers
  the former, so a re-announce that heals a lost Home Assistant-side
  registration would silently leave your hand-built entity behind.
- `send_state_object()` is for states that aren't a single scalar (this is
  what `trackers:` uses internally for latitude+longitude). For ordinary
  values use `send_state_float()` / `send_state_bool()` / `send_state_string()`.
- `send_entity_declare()`'s last argument adds platform-specific declare fields;
  pass `nullptr` when there are none, or a lambda taking a `JsonObject` (that's
  how `select` sends its `options`, `number` its `min`/`max`/`step`, and so on).
- Everything is a plain no-op while disconnected, so these are safe to call from
  any interval or trigger.

See the integration's
[PROTOCOL.md](https://github.com/eigger/hass-ws-bridge/blob/main/docs/PROTOCOL.md)
for the full set of declarable platforms and their fields.

## Removing stale entities

Home Assistant keeps every entity this gateway has ever declared. Deleting a
sensor from your YAML does **not** remove it from HA — it just stops being
updated, and with `keep_last_state_on_disconnect: true` it even keeps looking
alive after a restart, because the integration restores it from its stored
definition.

Set `sync_entities: true` and the component sends `ws_bridge/sync` with every
`unique_id` it declared, right after connecting. HA removes the ones that are
no longer in the list:

```yaml
ws_bridge:
  host: 192.168.0.10
  token: !secret ha_token
  sync_entities: true
```

Entities that *are* still declared are left completely alone — their
`entity_id`, history, and long-term statistics survive. That is why the
component syncs rather than removing everything and redeclaring.

**When not to enable it.** The list is built from everything declared during
the connect pass: `platform: ws_bridge` entities (including `*_id:` wrapping),
`entities:`, `trackers:`, and lambdas in `on_declare:` or `on_connected:`.
Anything that declares itself *later* — from an `interval:`, a button press, a
sensor callback — will be missing from the list and get deleted from HA. Leave
`sync_entities` off in that case and remove those entities by hand instead.

The sync is sent once per connection, not on every `reannounce_interval`. If
nothing has gone stale, HA does nothing and no entity is touched.

Requires the `ws_bridge` integration v1.3.0 or newer on the HA side. Older
versions reject the unknown command and log an error; nothing else breaks.

## Behavior / Limitations

- **Read-only platforms** (`sensor`, `binary_sensor`, `text_sensor`) push
  their state to Home Assistant automatically whenever it changes.
- **Controllable platforms** (`switch`, `number`, `select`, `button`, `update`)
  receive commands from Home Assistant. A plain (non-wrapping) entity updates
  its own state optimistically (`publish_state`/`this->state`) — hook
  `on_turn_on`/`lambda:`/etc. in your own YAML if you need to drive real
  hardware from that. A wrapping entity (`switch_id:` / `number_id:` /
  `select_id:` / `button_id:` / `update_id:`, or an `entities:` entry)
  forwards the command to the source instead.
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
