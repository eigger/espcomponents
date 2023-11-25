### add esphome *.yaml
```
external_components:
  - source: github://eigger/espcomponents
    components: [ bbq10_keyboard ]

i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

bbq10_keyboard:
  key:

```
