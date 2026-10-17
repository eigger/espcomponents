# IP5306 Battery Charger

Reads battery and charger status from an I2C-capable IP5306. Requires an I2C bus;
all exposed entities are optional.

## Configuration

Merge into your device YAML, using the SDA/SCL pins wired to the IP5306:

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ip5306]

i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

ip5306:
  address: 0x75
  battery_level:
    name: "Battery Level"
  charger_connected:
    name: "Charger Connected"
  charge_full:
    name: "Charge Full"
  poweroff:
    name: "Power Off"
```

| Option | Description |
| --- | --- |
| `address` | I2C address; default `0x75` |
| `i2c_id` | Selects the bus when multiple I2C buses are configured |
| `battery_level` | Coarse battery level in %, from the chip's level register |
| `charger_connected` | Binary sensor for charger connection |
| `charge_full` | Binary sensor for full-charge indication |
| `poweroff` | Button to switch off the power output |

Use binary sensor `on_press` / `on_release` automations for charger notifications.
This component is not a polling component and has no `update_interval` option.
If no values appear, check that the board exposes the IP5306 I2C interface at `0x75`.

[Other components](../README.md)
