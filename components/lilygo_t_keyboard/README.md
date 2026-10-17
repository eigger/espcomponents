### add esphome *.yaml
```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ lilygo_t_keyboard ]

lilygo_t_keyboard:
  press_key:
    name: press key
  release_key:
    name: release key
```
<img src="../../docs/images/hardware/lilygo-t-keyboard.jpg" width="400">
