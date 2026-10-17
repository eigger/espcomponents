# Lingan SWA1 Plug

Relay, push-button, and LED configuration. The button toggles the relay, whose
restore mode is `RESTORE_DEFAULT_OFF`.

## Configuration

The preset uses ESP8266 PWM but does not define a board. Add the board definition
for your hardware, then combine it with the shared Wi-Fi base:

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/plug/lingan/swa1.yaml
```

Provide `name`, `friendly_name`, and Wi-Fi/OTA secrets using the
[shared setup](../../README.md#common-device-setup).

| Function | GPIO |
| --- | --- |
| Button, inverted pull-up | GPIO13 |
| Relay | GPIO5 |
| LED, inverted PWM | GPIO4 |

Verify these pins against your board revision before using the relay configuration.

[All packages](../../README.md)
