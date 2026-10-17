# Components

Reusable ESPHome external components. Each component README contains its
configuration and hardware requirements. Add examples to an existing device YAML
with a board, networking, and Home Assistant API as needed.

| Component | Purpose |
| --- | --- |
| [uartex](uartex) | UART / RS-485 protocol entities |
| [tcp_server](tcp_server) | Raw TCP server and UART forwarding |
| [sip_client](sip_client) | SIP calls and intercom audio |
| [ws_bridge](ws_bridge) | Home Assistant entities over WebSocket (ESP32 / ESP-IDF) |
| [ble_elm327](ble_elm327) | BLE OBD-II telemetry |
| [jaalee_jht](jaalee_jht) | BLE temperature and humidity |
| [axp192](axp192) | I2C power management |
| [ip5306](ip5306) | I2C battery and charger status |
| [bbq10_keyboard](bbq10_keyboard) | I2C BBQ10 keyboard |
| [lilygo_t_keyboard](lilygo_t_keyboard) | Fixed GPIO keyboard matrix (Arduino) |
| [tca8418_keyboard](tca8418_keyboard) | I2C M5Cardputer ADV keyboard |
| [m5unit_scales](m5unit_scales) | I2C scale, tare, LED, and filter controls |
| [bmi270](bmi270) | I2C accelerometer and gyroscope |
| [bmm150](bmm150) | I2C magnetometer and calibrated heading |
| [divoom](divoom) | BLE display |

Load only the components you use:

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [uartex]
```

`@latest` follows the release tag maintained by this repository. Use `@master`
for development changes or a specific release tag for a fixed version. Component
examples are configuration fragments unless they explicitly include a full device setup.

[Device packages](../packages/README.md) · [Validation and tests](../tests/README.md) · [Project overview](../README.md)
