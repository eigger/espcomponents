# Jaalee JHT BLE Sensor

Receives Jaalee JHT temperature, humidity, battery, and signal-strength data from
BLE advertisements. Requires an ESP32 with BLE and `esp32_ble_tracker`; no
`ble_client` connection is needed.

## Configuration

Add this to your ESP32 device YAML and replace the example MAC with your sensor's address.

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [jaalee_jht]

esp32_ble_tracker:

sensor:
  - platform: jaalee_jht
    mac_address: "AA:BB:CC:DD:EE:FF"
    temperature:
      name: "Jaalee Temperature"
    humidity:
      name: "Jaalee Humidity"
    battery_level:
      name: "Jaalee Battery"
    signal_strength:
      name: "Jaalee RSSI"
```

| Option | Required | Description |
| --- | --- | --- |
| `mac_address` | Yes | BLE address of the JHT sensor |
| `temperature` | No | Temperature in °C |
| `humidity` | No | Relative humidity in % |
| `battery_level` | No | Battery level in % |
| `signal_strength` | No | Received signal strength in dBm |

Updates follow received advertisements rather than an `update_interval`. To use
multiple sensors, add one `platform: jaalee_jht` entry per MAC address. If readings
stay unknown, confirm the address, sensor power, and BLE reception in the logs.

[Other components](../README.md)
