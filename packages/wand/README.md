# Magic Caster Wand

BLE configuration for a Harry Potter Magic Caster Wand. Exposes connection and
button states, battery level, spell text, and calibration / LED / buzzer controls.

## Configuration

Use an ESP32 with BLE; this package does not define a board. Set the wand's BLE
address (`MCW-XXXX` device) in your device YAML:

```yaml
substitutions:
  name: magic-caster-wand
  friendly_name: "Magic Caster Wand"
  mcw_mac_address: "AA:BB:CC:DD:EE:FF"

esp32:
  board: esp32dev  # Replace with your board
  framework:
    type: esp-idf

packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/wand/magic_caster_wand.yaml

# Required when validating with ESPHome 2026.9.1
sensor:
  - id: !extend mcw_battery
    type: characteristic
```

Provide Wi-Fi/OTA secrets using the
[shared setup](../README.md#common-device-setup). Override `name` for the shared
base; the wand's `device_name` substitution does not set the base's device name.

The preset omits the battery sensor subtype. The `!extend` block above supplies
it in your device YAML; the example passes configuration validation with ESPHome
2026.9.1.

If the wand does not connect, check its battery, MAC address, and existing BLE
connections. The preset sends button-sensitivity commands after connecting.

[All packages](../README.md)
