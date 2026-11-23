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
| OV3660 | JPEG camera at 640 × 480, with up to 10 fps during streaming and 0.1 fps while idle. |
| Side reset button | Wired to ESP_EN. A short press resets the ESP32. |
| BMI270 | IMU temperature in Home Assistant. Acceleration and gyroscope axes are internal. |
| BMM150 | Three internal compensated magnetic field axes in µT via the BMI270 auxiliary bus |
| IR LED (GPIO47) | ESPHome action `transmit_ir_raw` for 38 kHz raw waveforms |
| Green download LED | Wired to the separate power/download controller. ESPHome cannot switch it independently. |

The GPIO41 `USER_BUT` net has a pull-up and no switch on the M12 schematic.
The side switch drives `ESP_EN` directly, so it cannot report a button event to ESPHome.

Use the internal axis sensors in ESPHome automations or lambdas. Their IDs are
`atoms3r_m12_accel_{x,y,z}`, `atoms3r_m12_gyro_{x,y,z}`, and
`atoms3r_m12_mag_{x,y,z}`. For example, read X-axis acceleration in g with
`id(atoms3r_m12_accel_x).state`. The nine axes are hidden from Home Assistant.

Override camera settings in your device YAML to use full-resolution snapshots.

```yaml
esp32_camera:
  resolution: 2048x1536
  max_framerate: 0.5 fps
  idle_framerate: 0 fps
```

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
