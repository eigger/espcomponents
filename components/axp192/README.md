# AXP192 Power Management

I2C driver for battery status, brightness control, and power-off on AXP192-based
hardware. Setup programs power rails and charging registers; use it only with a
board whose AXP192 wiring matches [the implementation](axp192.cpp).

## Configuration

Add to your device YAML; SDA/SCL below are example pins.

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [axp192]

i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

axp192:
  address: 0x34
  update_interval: 30s
  battery_level:
    name: "Battery Level"
  battery_state:
    name: "Battery Present"
  battery_charging:
    name: "Battery Charging"
  brightness:
    name: "Brightness"
  poweroff:
    name: "Power Off"
```

| Option | Default / purpose |
| --- | --- |
| `address` | `0x34` |
| `update_interval` | `60s` |
| `i2c_id` | Select a bus when multiple I2C buses are configured |
| `battery_level` | Optional voltage-based estimate in %; not a fuel-gauge measurement |
| `battery_state` / `battery_charging` | Optional battery status binary sensors |
| `brightness` | Optional number, 0–100, controlling the configured power output |
| `poweroff` | Optional button that switches off device power |

Battery percentage is approximate; the source notes that the estimate needs
improvement. If the chip is not detected, check its address and your board's I2C pins.

[Other components](../README.md)
