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
  - address: 0x1f
    id: id_bbq10
    key:
      id: id_key
    key_state:
      id: id_key_state
    pressed_key:
      id: id_pressed_key

```
