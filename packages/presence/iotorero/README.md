# Iotorero Presence Sensors

ESPHome configurations for the **Iotorero** series of human presence sensors. These sensors are based on the ESP32-C3 microcontroller and provide advanced presence detection capabilities using mmWave and PIR technology, along with integrated light sensing.

## Supported Devices

### 1. Standard Human Presence Sensor (`ps01_c3.yaml`)
A comprehensive configuration featuring a combination of PIR and mmWave sensors, as well as a BH1750 light sensor.

**Key Features:**
- **Microcontroller**: ESP32-C3
- **mmWave Sensor**: Radar-based detection with configurable sensitivity, delays, and range via UART.
- **PIR Sensor**: Supplementary motion detection.
- **Illuminance**: BH1750 light sensor for ambient light levels.
- **Onboard Interfaces**: Status LED and a physical button for factory reset.

### 2. Multi-Zone Human Presence Sensor (`ps02_c3_mz.yaml`)
An advanced configuration utilizing the LD2450 radar for multi-target, multi-zone tracking.

**Key Features:**
- **Microcontroller**: ESP32-C3
- **LD2450 mmWave Radar**: Capable of tracking up to 3 separate targets simultaneously, reporting X/Y coordinates, speed, and resolution.
- **Zone Configuration**: Supports monitoring up to 3 customized detection zones.
- **Illuminance**: BH1750 light sensor.
- **Bluetooth Proxy**: Supports BLE proxying for Home Assistant.
- **Onboard Interfaces**: Physical button for LD2450 radar reset and ESP32 reboot.

---

## Reference Models

These Iotorero devices share similarities in form factor and operation with Athom presence sensors. For reference and general configuration ideas, please see the following models:

- [Athom Multi-Zone Human Presence Sensor](https://www.athom.tech/blank-1/multi-zone-human-presence-sensor)
- [Athom Human Presence Sensor](https://www.athom.tech/blank-1/human-presence-sensor)
- [Athom Human Presence Sensor Variant](https://www.athom.tech/blank-1/human-presence-sensor-1)

## Purchase Link

The Iotorero presence sensors can be purchased directly from their official AliExpress store:

- 🛒 [**Iotorero Official Store on AliExpress**](https://iotorero.ko.aliexpress.com/store/1104532047)