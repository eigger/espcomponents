# Bluetooth proxies

ESP32 Bluetooth proxy presets for Home Assistant. Choose one preset and one
network base; each device preset includes its ESP32 target.

| File | Network / hardware |
| --- | --- |
| [`bluetooth_proxy.yaml`](bluetooth_proxy.yaml) | Generic ESP32, Wi-Fi |
| [`m5stack/atom_s3.yaml`](m5stack/atom_s3.yaml) | AtomS3, Wi-Fi |
| [`lilygo/t_dongle_s3.yaml`](lilygo/t_dongle_s3.yaml) | T-Dongle S3, Wi-Fi; display configuration is commented out |
| [`gl_inet/gl_s10.yaml`](gl_inet/gl_s10.yaml) | GL-S10 V2.3, IP101 Ethernet; LEDs, reset button, and I2C |
| [`seeed/xiao.yaml`](seeed/xiao.yaml) | ESP32-S3 + W5500 Ethernet; source specifies boards produced after 2025-11-01 |

## Wi-Fi example

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/bluetooth_proxy/m5stack/atom_s3.yaml
```

Provide `name`, `friendly_name`, and Wi-Fi/OTA secrets as described in the
[shared device setup](../README.md#common-device-setup).

## Ethernet example

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base_wired.yaml
      - packages/bluetooth_proxy/gl_inet/gl_s10.yaml
```

Provide `name`, `friendly_name`, and `ota_password`. The GL-S10 preset extends the
wired base's `sensor_status` entity, so include the base or define that ID yourself.
Do not also include the Wi-Fi base for these Ethernet-only configurations.

If discovery fails, check the API connection to Home Assistant and confirm that
the board/PHY and Ethernet pins match the selected hardware revision.

[All packages](../README.md)
