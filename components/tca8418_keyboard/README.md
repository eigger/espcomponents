### TCA8418 Keyboard (M5Cardputer ADV)
TCA8418 I2C keypad scan IC, mapped to the M5Cardputer ADV 4x14 keyboard layout.

### add esphome *.yaml
```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ tca8418_keyboard ]

i2c:
  sda: 8
  scl: 9

tca8418_keyboard:
  address: 0x34
  model: cardputer_adv   # key layout / scan remap (default)
  irq_pin:
    number: 11
    mode: INPUT
  # Automation triggers (the pressed key string is passed as `x`)
  on_key_press:
    - lambda: |-
        ESP_LOGI("kb", "pressed: %s", x.c_str());
  on_key_release:
    - lambda: |-
        ESP_LOGI("kb", "released: %s", x.c_str());
  # Optional text sensors (e.g. to show the last key in Home Assistant)
  press_key:
    name: press key
  release_key:
    name: release key
```
- `model` selects the key map and the scan->layout remap. Currently `cardputer_adv` (M5Cardputer ADV) is supported.
- The TCA8418 scans the matrix and debounces in hardware; this component only drains the chip's key-event FIFO, so the loop never blocks.
- `irq_pin` is optional. Without it, the key event FIFO is polled over I2C every loop. With it, I2C traffic is skipped while no key event is pending.
- `on_key_press` / `on_key_release` are the convenient way to react to keys in automations; `press_key` / `release_key` text sensors are optional and mainly for Home Assistant display. Modifier-only keys (`Fn`, `Shift`, `Ctrl`, `Opt`, `Alt`) do not fire events on their own — they shape the emitted key string (e.g. `Ctrl+c`, `F1`, `Up`).
- `Fn` + key produces `Esc`, `F1`~`F12`, `Del` and arrow keys. `Ctrl` / `Opt` / `Alt` are added as prefixes (e.g. `Ctrl+c`).

<img src="https://static-cdn.m5stack.com/resource/docs/products/core/Cardputer-Adv/img-bcc8b8e9-fee9-4239-a2d8-66708cf868de.webp" width="400">
