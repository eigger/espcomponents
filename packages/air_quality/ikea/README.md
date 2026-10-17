# IKEA VINDRIKTNING

D1 Mini configuration for the PM1006 particulate sensor. Publishes PM2.5 and a
text classification using thresholds defined in the YAML.

## Configuration

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/air_quality/ikea/vindriktning_d1_mini.yaml
```

Set the device substitutions and Wi-Fi/OTA secrets using the
[shared setup](../../README.md#common-device-setup). The preset includes the
ESP8266 D1 Mini board definition.

| Setting | Value |
| --- | --- |
| UART RX / TX | D4 / D5 |
| Baud rate | `9600` |
| PM1006 update interval | `5min` |
| PM2.5 processing | Heartbeat and moving-average filters |

The `Good` / `Moderate` / `Unhealthy` labels are this preset's own PM2.5 thresholds,
not an AQI calculation. If readings are missing, inspect the UART wiring and logs.

[All packages](../../README.md)
