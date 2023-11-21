### add esphome *.yaml
```
external_components:
  - source: github://eigger/espcomponents
    components: [ bbq10_keyboard ]

i2c:
  sda: 21
  scl: 22
  scan: true

bbq10_keyboard:
  - address: 0x1f

```
