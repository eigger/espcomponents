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
    name: Key
  last_key:
    name: Last Key

```
<img src="https://lilygo.cc/cdn/shop/products/Q374-watch-keyboard-lilygo.jpg?v=1679983894" width="400">
