# Colorado Tab5 Dashboard

ESPHome configuration for a vehicle dashboard based on the M5Stack Core TAB5 (ESP32-P4 + ESP32-C6). This dashboard provides real-time vehicle telemetry via an OBD2 Bluetooth adapter, high-precision GNSS tracking, vehicle inclinometer (pitch/roll), cabin & cargo climate tracking using BLE sensors, and comprehensive air quality monitoring (CO2 & TVOC).

## Preview

### In-Vehicle Dashboard
<p align="center">
  <img src="../../../documents/colorado/dash.jpg" width="720" alt="Colorado Tab5 In-Vehicle Dashboard">
</p>

### Hardware Assembly
| Front View (Tab5 & GPS Antenna) | Rear View (Module GNSS, Unit HUB, SCD40, SGP30) |
| :---: | :---: |
| <img src="../../../documents/colorado/assembly_front.jpg" width="360" alt="Colorado Tab5 Assembly Front"> | <img src="../../../documents/colorado/assembly_rear.jpg" width="360" alt="Colorado Tab5 Assembly Rear"> |

### Hardware Components

| 1. M5Stack Core TAB5 | 2. Vgate vLinker MC+ BLE | 3. M5Stack Module GNSS | 4. Jaalee JHT BLE Sensor |
| :---: | :---: | :---: | :---: |
| <img src="../../../documents/colorado/tab5.webp" width="200" alt="M5Stack Core TAB5"> | <img src="../../../documents/colorado/vlinker.jpg" width="200" alt="vLinker MC+ BLE OBD2"> | <img src="../../../documents/colorado/gnss.webp" width="200" alt="M5Stack Module GNSS"> | <img src="../../../documents/colorado/jaalee.png" width="200" alt="Jaalee JHT BLE Sensor"> |

| 5. M5Stack Unit CO2 (SCD40) | 6. M5Stack Unit TVOC (SGP30) | 7. M5Stack Unit HUB (1-to-3) |
| :---: | :---: | :---: |
| <img src="../../../documents/colorado/co2.webp" width="200" alt="M5Stack Unit CO2 (SCD40)"> | <img src="../../../documents/colorado/tvoc.webp" width="200" alt="M5Stack Mini Unit TVOC/eCO2 (SGP30)"> | <img src="../../../documents/colorado/hub.webp" width="200" alt="M5Stack Unit HUB 1 to 3"> |

## Features

- **Live Vehicle Dashboard**: High-resolution (1280x720) LVGL UI featuring real-time speedometer, gear/PRND indicator, dynamic gauge bars, pitch/roll inclinometer with vehicle tilt icons, and intelligent warning popups (e.g., Drowsiness Warning on high CO2, Overheating, High RPM, Low Fuel, Rapid Acceleration/Braking).
- **Vehicle Telemetry (OBD2)**: Integrates with the custom [ble_elm327](/components/ble_elm327) component to connect to a Vgate vLinker MC+ BLE OBD2 adapter, exposing Engine RPM, Speed, Engine Load, Coolant Temperature, Transmission Fluid Temperature, Oil Pressure, Intake Air Temperature, Fuel Level (% and GM liters), Car Battery Voltage, Odometer, Trip Distance, Gear Position, PRND status, and Runtime.
- **GNSS & Location Tracking**: Integrates the M5Stack Module GNSS (NEO-M9N-00B) via UART and BMP280 barometer via I2C, providing real-time GPS coordinates, speed, altitude, satellite count, HDOP, and Home Assistant `device_tracker` integration.
- **Vehicle Inclinometer**: Real-time vehicle pitch and roll angles calculated dynamically using the onboard 6-axis BMI270 IMU sensor.
- **Climate Monitoring**: Tracks cabin and cargo-bed (적재함) temperature, humidity, and battery levels using the custom [jaalee_jht](/components/jaalee_jht) BLE component.
- **Air Quality & Health Safety**: Monitors CO2, eCO2, and TVOC using Sensirion SCD40 and SGP30 I2C sensors connected via the M5Stack Unit HUB, providing real-time cabin air status and drowsiness alerts.
- **Power Management**: Monitors power consumption using INA226 and controls power peripherals (USB power, Quick Charge, Speakers, etc.) via PI4IOE5V6408 I2C GPIO expanders.
- **Home Assistant Real-time Sync**: Outbound WebSocket bridge via [ws_bridge](/components/ws_bridge) component, pushing 30+ telemetry, GNSS, environmental, and diagnostic entities directly to Home Assistant without needing an MQTT broker or VPN.
- **Garage Integration**: Seamless integration with [hass-garage](https://github.com/eigger/hass-garage) and [Garage](https://github.com/eigger/garage) server for automatic trip logging, fuel tracking, maintenance intervals, and parking location sync.

## Configuration Usage

Add the following to your ESPHome configuration:

```yaml
substitutions:
  name: "esp-colorado-tab5"
  friendly_name: "ESP Colorado TAB5"
  number: "12가 1234"
  mac_vlinker: "C0:25:E8:53:2C:90"
  mac_cabin_jht: "DA:E8:DD:E2:9A:47"
  mac_bed_jht: "F5:A8:DB:76:1A:F5"

packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents/
    files:
      - packages/display/colorado/colorado-tab5.yaml
```

## Home Assistant Integration (`ws_bridge`)

The dashboard connects directly to Home Assistant's `/api/websocket` endpoint over an outbound secure WebSocket (`wss://`) using the custom [ws_bridge](/components/ws_bridge) component and the companion [`hass-ws-bridge`](https://github.com/eigger/hass-ws-bridge) Home Assistant integration.

- **No MQTT / No VPN Needed**: Outbound WSS connection works seamlessly across mobile hotspots, home Wi-Fi, or remote tunnels (e.g., Nabu Casa / Cloudflare).
- **Persistent State**: With `keep_last_state_on_disconnect: true`, vehicle telemetry remains visible in Home Assistant even when the vehicle is parked and offline.
- **Auto Entity Pruning**: `sync_entities: true` automatically removes old or renamed entities on connection while preserving history and long-term statistics.
- **Synchronized Entities (30+)**:
  - **Vehicle Telemetry**: Speed, RPM, Engine Load, Throttle, Acceleration, Gear, PRND, Oil Pressure, Coolant Temp, Transmission Fluid Temp, Intake/Ambient Temp, Fuel Level (% & Liters), Odometer, Trip Distance, Engine Runtime, and Car Battery Voltage.
  - **GNSS & Location**: GPS Latitude & Longitude (`device_tracker`), GPS Speed, Altitude, Satellites Count, HDOP, Barometric Pressure & Temperature (BMP280).
  - **Inclinometer**: Vehicle Pitch and Roll angles (°).
  - **Cabin & Cargo Climate**: Jaalee JHT temperature, humidity, and battery levels for both cabin and cargo bed (적재함).
  - **Air Quality**: SCD40 CO2/temp/humidity, SGP30 eCO2 & TVOC.
  - **Firmware Update**: Exposes the `ota_update` entity as a native Home Assistant Update card with one-click install.

## OTA Updates (ESPHome OTA Publisher)

To enable remote Over-The-Air (OTA) firmware updates via Home Assistant without exposing local ports or using a VPN, this configuration integrates with the **[ESPHome OTA Publisher](https://github.com/eigger/hassio-apps/tree/master/esphome_ota)** add-on.

1. **Install Add-on**: Install [ESPHome OTA Publisher](https://github.com/eigger/hassio-apps/tree/master/esphome_ota) in Home Assistant.
2. **Register Device**: Register `esp-colorado-tab5` in the add-on UI and build/publish firmware.
3. **OTA Package**: The add-on generates `ota_server/devices/esp-colorado-tab5.yaml` (or `ota_server/update.yaml`), which defines `ota_update` and sets `esphome.project.version`.
4. **Version Management**: The dashboard automatically uses `ESPHOME_PROJECT_VERSION` (`*version`) across boot logs, header version label, splash screen, and `ws_bridge` device info.
5. **Home Assistant Update Entity**: `ws_bridge` exposes `ota_update` to Home Assistant as a native firmware Update entity with install/check features.

## Required Secrets

Make sure you have the following defined in your `secrets.yaml`:

```yaml
# Vehicle Wi-Fi / Hotspot (Primary, priority 10)
colorado_wifi_ssid: "your_vehicle_hotspot_ssid"
colorado_wifi_password: "your_vehicle_hotspot_password"

# Home / Backup Wi-Fi (Secondary, priority 5)
wifi_ssid: "your_home_wifi_ssid"
wifi_password: "your_home_wifi_password"

# Home Assistant WebSocket Bridge (ws_bridge)
ha_address: "homeassistant.yourdomain.com"
ha_token: "your_long_lived_access_token"
```


## ble_elm327 Setup (vLinker OBD2)

Production OBD configuration from the Colorado Tab5 dashboard. Tested with a **vLinker MC+** BLE adapter on a **Chevrolet Colorado Z71** (ISO 15765-4 CAN 500 kbps).

### Package include (OBD sensors only)

Add `mac_vlinker` to your substitutions, then include the OBD package. The parent config must already define `esp32`, `esp32_hosted`, and `esp32_ble_tracker` (see `colorado-tab5.yaml` for Tab5 pinout).

```yaml
substitutions:
  mac_vlinker: "C0:25:E8:53:2C:90"   # replace with your adapter MAC

packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents/
    files:
      - packages/display/colorado/colorado-ble-elm327.yaml
```

Source: [`colorado-ble-elm327.yaml`](colorado-ble-elm327.yaml)

### Minimal standalone example

For a new ESP32 board (non-Tab5: omit `esp32_hosted` and use the board’s native BLE):

```yaml
substitutions:
  mac_vlinker: "AA:BB:CC:DD:EE:FF"

external_components:
  - source: github://eigger/espcomponents
    components: [ ble_elm327 ]
    refresh: always

esp32_ble_tracker:

ble_client:
  - mac_address: ${mac_vlinker}
    id: obd_client

ble_elm327:
  id: obd_elm
  ble_client_id: obd_client
  service_uuid: "18F0"    # vLinker MC+
  rx_char_uuid: "2AF0"
  tx_char_uuid: "2AF1"
  init_commands:
    - "ATSP6"
  tx_delay: 50

sensor:
  - platform: ble_elm327
    name: "Engine RPM"
    preset: rpm
    update_interval: 1s
  - platform: ble_elm327
    name: "Car Speed"
    preset: speed
    update_interval: 1s
  - platform: ble_elm327
    name: "Engine Coolant Temperature"
    preset: coolant_temp
    update_interval: 10s
```

| Setting | vLinker MC+ (Colorado) | Default (many adapters) |
|---------|------------------------|-------------------------|
| `service_uuid` | `18F0` | `FFF0` |
| `rx_char_uuid` | `2AF0` | `FFF1` |
| `tx_char_uuid` | `2AF1` | `FFF2` |
| `init_commands` (extra, after base) | `ATSP6` | omit or `[]` (base only) |

GM extended presets (`gm_fuel_level_liters`, `gm_current_gear`, `gm_prnd_status_alt`, `gm_oil_pressure`, `gm_trans_temp`, …) are vehicle-specific. See the [ble_elm327 component docs](/components/ble_elm327/README.md) for the full preset list and troubleshooting.

### Tab5 BLE prerequisite

M5Stack Tab5 routes BLE through an ESP32-C6 co-processor. Include `esp32_hosted` before `esp32_ble_tracker` — copy the block from [`colorado-tab5.yaml`](colorado-tab5.yaml) (lines 106–116).

## Hardware Configurations

### Main Board
- **Board**: M5Stack Core TAB5 (`esp32-p4-evboard` + ESP32-C6 Hosted BLE/WiFi)
- **Display**: 5.0" 1280x720 IPS Capacitive Touch LCD (MIPI-DSI ST7703 + FT5x06)
- **I2C Bus (Internal / `bsp_bus`)**: SDA GPIO31, SCL GPIO32 (400kHz)
- **I2C Bus (External Port.A / `ext_bus`)**: SDA GPIO53, SCL GPIO54 (100kHz)

### GNSS & Expansion Modules
- **GNSS Module**: M5Stack Module GNSS (NEO-M9N-00B on UART RX GPIO51 / TX GPIO47, PPS GPIO16)
- **Barometer / Altimeter**: BMP280 on internal I2C bus (`bsp_bus`, address `0x76`)
- **I2C Expansion**: M5Stack Unit HUB (HY2.0-4P 1-to-3 Hub connected to Port.A `ext_bus`)
- **CO2 Sensor**: M5Stack Unit CO2 (Sensirion SCD40, I2C address `0x62` on `ext_bus`)
- **TVOC / eCO2 Sensor**: M5Stack Mini Unit TVOC/eCO2 (Sensirion SGP30, I2C address `0x58` on `ext_bus`)

### BLE Devices
Configure your device MAC addresses via `substitutions` variables as shown in the Configuration Usage.
- **OBD2 BLE Adapter**: Vgate vLinker MC+ (`mac_vlinker`)
- **Cabin Climate Sensor**: Jaalee JHT BLE Beacon (`mac_cabin_jht`)
- **Cargo Bed Climate Sensor**: Jaalee JHT BLE Beacon (`mac_bed_jht`, UI label 적재함)
- **External Sensors**: Parses `035D` Manufacturer Data (e.g., parking remote / sensors)

### Onboard ICs & Peripherals
- **PI4IOE5V6408 (I2C `0x43`, `0x44`)**: GPIO Expansion handling USB power, Quick charge, external 5V, speaker enable, WiFi antenna switching, charge status, and headphone detection.
- **INA226 (I2C `0x41`)**: Battery voltage, bus current, and power monitoring.
- **BMI270 (I2C `0x68`)**: 6-axis IMU (Accelerometer & Gyroscope) for vehicle pitch and roll orientation calculation.
- **RX8130CE (I2C `0x32`)**: Real-Time Clock with backup battery support.

## Garage Integration (`hass-garage`)

Vehicle telemetry collected by this dashboard can be automatically forwarded to the self-hosted [Garage](https://github.com/eigger/garage) vehicle management server using the **[hass-garage](https://github.com/eigger/hass-garage)** Home Assistant custom integration.

- **Zero YAML Configuration**: No need for manual `rest_command` or complex automations. Configure entirely through the Home Assistant UI.
- **Automatic Telemetry Stream**: Select the entities exposed by `ws_bridge` (Speed, RPM, Fuel Level, Odometer, and your phone's GPS `device_tracker`), and `hass-garage` automatically syncs telemetry to Garage whenever driving metrics change.
- **Two-way Sync**: Garage service reminders (due/upcoming maintenance) and last known parking location are automatically published back to Home Assistant sensors.
