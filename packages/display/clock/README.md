# TM1637 Clock

D1 Mini preset for a TM1637 display. Shows hours and minutes with a blinking
separator, using the `sntp_time` clock supplied by the shared base.

## Configuration

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/display/clock/tm1637_d1_mini.yaml
```

Provide `name`, `friendly_name`, and Wi-Fi/OTA secrets using the
[shared setup](../../README.md#common-device-setup). The package sets the D1 Mini
board; CLK is **D6** and DIO is **D5**.

If you supply your own base, define a time component with `id: sntp_time`.
The shared base uses `Asia/Seoul`; override the time zone for your location.

[All packages](../../README.md)
