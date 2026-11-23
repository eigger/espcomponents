# M5Stack AtomS3R-M12

ESPHome package for the C126-M12 with the OV3660 camera. Requires ESPHome 2026.11.0 or newer
for built-in BMM150 support. Use the shared
[device setup](../../README.md#common-device-setup) for Wi-Fi, API, OTA,
and diagnostics.

```yaml
substitutions:
  name: esp-atom-camera
  friendly_name: "ESP Atom Camera"

packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/esp32.yaml
      - packages/camera/m5stack/atoms3r_m12.yaml
```

| Hardware | Home Assistant |
| --- | --- |
| OV3660 | Camera at 2048 × 1536. Periodic idle captures are disabled, and streams are limited to 0.5 fps. |
| User button (GPIO41) | Debounced binary sensor |
| BMI270 | Three accelerometer axes in g, three gyroscope axes in °/s, and IMU temperature |
| BMM150 | Three compensated magnetic field axes in µT via the BMI270 auxiliary bus |
| IR LED (GPIO47) | ESPHome action `transmit_ir_raw` for 38 kHz raw waveforms |
| Green download LED | Wired to the separate power/download controller. ESPHome cannot switch it independently. |

The IR action accepts a list named `timings`. Values are in microseconds.
Positive values are carrier pulses and negative values are pauses. For example,
send timings learned from your remote to the Home Assistant action
`esphome.esp_atom_camera_transmit_ir_raw`.

At boot, the package powers the camera through GPIO18 and checks that both
camera bus lines are high. After the bus is initialized, it starts the camera
clock and reads the OV3660 chip ID before letting the camera driver initialize.
Both checks have time limits so Home Assistant connectivity can still start
if a sensor fails to respond.

[M5Stack pin map and schematics](https://docs.m5stack.com/en/core/AtomS3R-M12) ·
[M5Stack ESPHome reference](https://docs.m5stack.com/en/homeassistant/cameras/atoms3r_m12)
