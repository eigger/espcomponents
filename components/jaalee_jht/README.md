### add esphome *.yaml
```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ jaalee_jht ]

esp32_ble_tracker:
sensor:
  - platform: jaalee_jht
    mac_address: 38:81:D7:0A:9C:11
    temperature:
      name: "Jaalee JHT Temperature"
    humidity:
      name: "Jaalee JHT Humidity"
    battery_level:
      name: "Jaalee JHT Battery Level"


```
