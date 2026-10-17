# Device packages

Device configurations and reusable YAML fragments. Choose a device below, then
follow its README for wiring and required substitutions.

| Category | Documentation / configuration |
| --- | --- |
| Wallpads / 월패드 | [Vendor selection and RS-485 pin maps](wallpad) |
| Air conditioning / 에어컨 | [Samsung NASA](hvac/samsung) |
| Vehicle dashboard | [Colorado / Tab5](display/colorado) |
| SIP intercoms | [Voice PE](sip/voice_pe) · [Atom Echo](sip/atom_echo) |
| Bluetooth proxies | [Wi-Fi and Ethernet variants](bluetooth_proxy) |
| Presence sensors | [Iotorero](presence/iotorero) |
| Buttons | [Neimz](button/neimz) |
| Scales | [Pet food bowl scale](scale) |
| Lighting | [Puppy LED lamp](led) |
| Radiation | [BR6 Geiger counter](radiation) |
| People counting | [Botem CSM505 — unverified](count/botem) |
| Air quality | [IKEA VINDRIKTNING](air_quality/ikea) |
| Clock | [TM1637 display](display/clock) |
| Louvers | [Verta variants](louver/verta) |
| Smart plug | [Lingan SWA1](plug/lingan) |
| Wand | [Magic Caster Wand](wand) |

## Common device setup

For a Wi-Fi device, combine the shared base with **one** device package:

```yaml
substitutions:
  name: puppy-led
  friendly_name: "Puppy LED"

packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/led/puppy_d1_mini.yaml
```

Create `secrets.yaml` beside your device YAML:

```yaml
wifi_ssid: "YOUR_WIFI_SSID"
wifi_password: "YOUR_WIFI_PASSWORD"
ota_password: "YOUR_OTA_PASSWORD"
```

| Shared file | Provides |
| --- | --- |
| [`device_base.yaml`](device_base.yaml) | Device name, Wi-Fi, API, OTA, time, and diagnostics |
| [`device_base_wired.yaml`](device_base_wired.yaml) | API, OTA, time, and diagnostics without Wi-Fi; add an Ethernet device package |
| [`esp32.yaml`](esp32.yaml) | Internal temperature sensor only; does **not** select a board |

Both bases require `name` and `friendly_name` substitutions. The wired base only
requires `ota_password` in secrets. Configure the board separately if the selected
device package does not include `esp32:` or `esp8266:`.

Examples use `ref: master` for the files in this tree. Use a release tag instead
when you need a fixed package version; verify that the tag contains the selected
files. Individual packages may load external components from their own release refs.
Do not include both network bases or unrelated device presets in the same device YAML.

## Validate your device

```bash
esphome config my-device.yaml
esphome compile my-device.yaml
```

Start with configuration validation after changing pins, room addresses, secrets,
or substitutions. The shared bases use `logger.level: ERROR`; temporarily override
it to `DEBUG` in the device YAML when troubleshooting.

[Component documentation](../components/README.md) · [Tests](../tests/README.md) · [Project overview](../README.md)
