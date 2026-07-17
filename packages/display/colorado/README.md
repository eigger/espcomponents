# Colorado Tab5 Dashboard

ESPHome configuration for a vehicle dashboard based on the ESP32-P4 EV Board (`esp32-p4-evboard`). This dashboard provides real-time vehicle telemetry via an OBD2 Bluetooth adapter, climate tracking using BLE sensors, and air quality monitoring.

## Preview

| 1. M5Stack Tab5 | 2. vLinker BLE OBD2 Adapter | 3. Jaalee JHT BLE Sensor |
| :---: | :---: | :---: |
| <img src="../../../documents/colorado/1.png" width="200" alt="M5Stack Tab5"> | <img src="../../../documents/colorado/2.jpg" width="300" alt="vLinker BLE OBD2"> | <img src="../../../documents/colorado/3.png" width="300" alt="Jaalee JHT"> |

### 4. Comprehensive Dashboard Results
| 4 | 5 |
| :---: | :---: |
| <img src="../../../documents/colorado/4.jpg" width="400" alt="Comprehensive Results 4"> | <img src="../../../documents/colorado/5.jpg" width="400" alt="Comprehensive Results 5"> |

## Features

- **Vehicle Telemetry**: Integrates with the custom [ble_elm327](/components/ble_elm327) component to connect to a vLinker BLE OBD2 adapter, exposing Engine RPM, Coolant Temperature, Fuel Level (% and GM liters), Engine Load, Speed, Odometer, Gear Position, PRND, and Runtime as native ESPHome sensors.
- **Climate Monitoring**: Tracks cabin and cargo-bed (적재함) temperature/humidity using the custom [jaalee_jht](/components/jaalee_jht) BLE component.
- **Air Quality**: Monitors CO2, eCO2, and TVOC using onboard SCD4x and SGP30 I2C sensors.
- **Dynamic UI**: LVGL-based UI with dynamic color changes based on sensor values and pop-up alerts for critical conditions (e.g., Overheating, High RPM, Low Fuel, Speeding, Drowsiness Warning via High CO2).
- **Power Management**: Monitors power consumption using INA226 and controls power peripherals (USB power, Quick Charge, Speakers, etc.) via PI4IOE5V6408 I2C GPIO expanders.

## Configuration Usage

Add the following to your ESPHome configuration:

```yaml
substitutions:
  name: "esp-colorado-tab5"
  friendly_name: "ESP Colorado TAB5"
  version: "v260602 rev.1"
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

## Required Secrets

Make sure you have the following defined in your `secrets.yaml`:

- `ota_password`
- `colorado_wifi_ssid`
- `colorado_wifi_password`
- `wifi_ssid`
- `wifi_password`
- `colorado_wg_address`
- `colorado_wg_private_key`
- `wg_peer_endpoint`
- `colorado_wg_peer_public_key`
- `mqtt_broker_local`
- `mqtt_user`
- `mqtt_password`


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
- **Board**: `esp32-p4-evboard`
- **I2C Bus (Internal)**: SDA GPIO31, SCL GPIO32
- **I2C Bus (External)**: SDA GPIO53, SCL GPIO54

### BLE Devices
Configure your device MAC addresses via `substitutions` variables as shown in the Configuration Usage.
- **OBD2 BLE Adapter**: `vLinker` (`mac_vlinker`)
- **Cabin Climate Sensor**: `Jaalee JHT` (`mac_cabin_jht`)
- **Cargo Bed Climate Sensor**: `Jaalee JHT` (`mac_bed_jht`, UI label 적재함)
- **External Sensors**: Parses `035D` Manufacturer Data (e.g., parking remote / sensors)

### Sensors & ICs
- **PI4IOE5V6408 (I2C 0x43, 0x44)**: GPIO Expansion handling USB power, Quick charge, external 5V, speaker enable, WiFi antenna switching, charge status, and headphone detection.
- **INA226 (I2C 0x41)**: Battery voltage and current monitoring.
- **SGP30 (I2C 0x58)**: eCO2 and TVOC air quality monitoring.
- **SCD4x**: High accuracy CO2 concentration, temp, and humidity polling.

## Garage Telemetry Auto-Sync

Automatically push vehicle telemetry data from Home Assistant to your [Garage](https://github.com/eigger/garage) service.

### 1. Define the `rest_command` in `configuration.yaml`

```yaml
rest_command:
  garage_send_telemetry:
    url: "http://192.168.0.247/api/ingest/telemetry"
    method: POST
    headers:
      Authorization: !secret garage_colorado_api
      Content-Type: "application/json"
    payload: >-
      {% set raw_data = {
        "lat": lat | float(none) if lat is defined else none,
        "lon": lon | float(none) if lon is defined else none,
        "speed": speed | float(none) if speed is defined else none,
        "rpm": rpm | float(none) if rpm is defined else none,
        "fuelLevel": fuelLevel | float(none) if fuelLevel is defined else none,
        "odometer": odometer | int(none) if odometer is defined else none,
        "dtcCodes": dtcCodes if dtcCodes is defined else none,
        "inVehicle": inVehicle if inVehicle is defined else none
      } %}
      
      {% set ns = namespace(result={}) %}
      {% for key, value in raw_data.items() %}
        {% if value is not none and value not in ['unknown', 'unavailable', ''] %}
          {% set ns.result = dict(ns.result, **{key: value}) %}
        {% endif %}
      {% endfor %}
      
      {{ ns.result | to_json }}
```

Add the Garage API key to `secrets.yaml`:

```yaml
garage_colorado_api: "your_api_key"
```

### 2. Automation — Submit Telemetry During Driving

```yaml
alias: Garage Send Colorado (Driving)
description: Send telemetry to Garage
triggers:
  - entity_id: device_tracker.sm_f966n
    trigger: state
conditions:
  - condition: template
    value_template: "{{ state_attr('device_tracker.sm_f966n', 'latitude') != None }}"
  - condition: template
    value_template: "{{ states('sensor.esp_colorado_tab5_engine_load') | float(0) > 0 }}"
actions:
  - action: rest_command.garage_send_telemetry
    data:
      lat: >-
        {{ state_attr('device_tracker.sm_f966n', 'latitude') | default('', true)
        }}
      lon: >-
        {{ state_attr('device_tracker.sm_f966n', 'longitude') | default('',
        true) }}
      speed: >-
        {% set car_speed = states('sensor.esp_colorado_tab5_car_speed') %}  {%
        set gps_speed = state_attr('device_tracker.sm_f966n', 'speed') %}  {% if
        car_speed not in ['unknown', 'unavailable', 'None', ''] %}
          {{ (car_speed | float(0)) | round(1) }}
        {% else %}
          {{ (gps_speed | float(0)) | round(1) if gps_speed not in [None, 'None', ''] else 0.0 }}
        {% endif %}
      rpm: "{{ states('sensor.esp_colorado_tab5_engine_rpm') | default('', true) }}"
      fuelLevel: "{{ states('sensor.esp_colorado_tab5_fuel_level') | default('', true) }}"
      odometer: >-
        {% set odo = states('sensor.esp_colorado_tab5_odometer') %}  {% if odo |
        is_number %}
          {{ odo | int }}
        {% endif %}
      inVehicle: true
mode: queued
```
