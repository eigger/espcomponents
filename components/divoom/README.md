# Divoom BLE Display

Draws to a Divoom display through a BLE client. The example targets Ditoo and
uses ESPHome's display lambda API. Requires an ESP32 with BLE support.

## Configuration

Merge into your ESP32 device YAML and replace the MAC address:

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [divoom]

esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"
    id: divoom_client

display:
  - platform: divoom
    ble_client_id: divoom_client
    model: ditoo
    update_interval: 1s
    lambda: |-
      it.fill(Color(255, 0, 0));
```

## Options

| Option | Default / purpose |
| --- | --- |
| `ble_client_id` | References the BLE client for the display |
| `model` | `ditoo`; the schema also lists `11x11` |
| `update_interval` | `1s` |
| `require_response` | `false`; whether BLE writes require a response |
| `time_id` | Optional ID of an existing time component |
| `version` / `status` | Version text sensor and BLE connectivity binary sensor |
| `type` / `brightness` | Clock-style selector and brightness number entity |

The default service UUID is `49535343-FE7D-4AE5-8FA9-9FAFD205E455`; the write
characteristic is `49535343-8841-43F4-A8D4-ECBE34729BB3`.

For text, define a `font:` entry pointing to a font you supply and call
`it.print(0, 0, id(your_font), "Hello");` inside the display lambda. No font files
are bundled with this component.

If the display stays blank, check the device MAC, BLE connection status, model,
and UUIDs. Release any connection from the vendor app before reconnecting.

[Other components](../README.md)
