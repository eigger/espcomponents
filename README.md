# ESPHome external components 
[![ESPHome External Component](https://img.shields.io/badge/ESPHome-External%20Component-000000?logo=esphome)](https://esphome.io/components/external_components/)
[![GitHub Release](https://img.shields.io/github/release/eigger/espcomponents.svg)](https://github.com/eigger/espcomponents/releases)
[![License](https://img.shields.io/github/license/eigger/espcomponents)](https://github.com/eigger/espcomponents/blob/master/LICENSE)

<a href="https://esphome.io/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://media.esphome.io/logo/logo-text-on-dark.svg", alt="ESPHome Logo">
    <img src="https://media.esphome.io/logo/logo-text-on-light.svg" alt="ESPHome Logo">
  </picture>
</a>

## 💬 Feedback & Support

🐞 Found a bug? Let us know via an [Issue](https://github.com/eigger/espcomponents/issues).  
💡 Have a question or suggestion? Join the [Discussion](https://github.com/eigger/espcomponents/discussions)!


## [uartex](/components/uartex)
**UART Extension Component**  
- Extends ESPHome UART integration for easier sensor data handling.
- Key feature: Seamless integration of sensors using UART data.
- Reference: [월패드 연동 가이드](https://github.com/eigger/espcomponents/wiki)

---

## [sip_client](/components/sip_client)
**SIP Voice Call Component**  
- Registers to a SIP PBX (Asterisk/FreePBX/3CX, etc.) and makes/receives voice calls.
- Two-way audio via ESPHome's standard `microphone`/`speaker` platforms (G.711 PCMU/PCMA, 8 kHz), with DTMF (RFC 2833).
- Ready-to-use package for Home Assistant Voice PE: [packages/sip/voice_pe](/packages/sip/voice_pe)

---

## [ws_bridge](/components/ws_bridge)
**WebSocket Bridge to Home Assistant (no MQTT needed)**  
- Connects directly to Home Assistant's `/api/websocket` and speaks the [`hass-ws-bridge`](https://github.com/eigger/hass-ws-bridge) protocol to declare `sensor`/`binary_sensor`/`switch`/`number`/`select`/`button` entities, push state, and receive commands — no MQTT broker required.
- ESP32 (ESP-IDF) only; uses ESP-IDF's `esp_websocket_client` for a secure `wss://` connection.

---

## [jaalee_jht](/components/jaalee_jht)
**Jaalee JHT Bluetooth Temperature and Humidity Sensor Component**  
- Supports Jaalee JHT Bluetooth Temperature and Humidity Sensor.

---

## [ble_elm327](/components/ble_elm327)
**BLE ELM327 OBD-II Reader Component**  
- Connects to Bluetooth LE ELM327 OBD-II adapters.
- Exposes vehicle data (RPM, speed, temperature, odometer, gear position, etc.) via robust built-in presets or custom parsing formulas.
- Fully integrates with standard ESPHome `ble_client` architecture.

---

## [axp192](/components/axp192)
**AXP192 Power Management Component**  
- ESPHome component for AXP192 power management chip.
- Provides power management and monitoring functions.
  
---

## [bbq10_keyboard](/components/bbq10_keyboard)
**Lilygo Watch Keyboard Component**  
- Supports BBQ10 Keyboard for Lilygo Watch.
- Product image:  
  <img src="https://lilygo.cc/cdn/shop/products/Q374-watch-keyboard-lilygo.jpg?v=1679983894" width="200">

---

## [lilygo_t_keyboard](/components/lilygo_t_keyboard)
**Lilygo T Keyboard Component**  
- Supports Lilygo T Keyboard.
- Product image:  
  <img src="https://cdn.shopify.com/s/files/1/0617/7190/7253/files/T-Keyboard.jpg?v=1708422205" width="200">
  
---

## [tca8418_keyboard](/components/tca8418_keyboard)
**TCA8418 Keyboard Component**  
- Supports TCA8418 I2C keypad scan IC (M5Cardputer ADV keyboard).
- Product image:  
  <img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1178/Cardputer-Adv_02.webp" width="200">

---

## [m5unit_scales](/components/m5unit_scales)
**M5Unit-Scales Component**  
- Supports M5Stack Unit-Mini Scales (SKU: U177) and Unit Scales.
- Allows weight measurements, LED control, button monitoring, calibration, and filter settings over I2C.
- Product images:  
  <img src="https://static-cdn.m5stack.com/resource/docs/products/unit/Unit-Mini%20Scales/img-e124306a-177f-4385-996d-18f10f928040.webp" width="200" alt="Unit-Mini Scales"> <img src="https://static-cdn.m5stack.com/resource/docs/products/unit/UNIT%20Scales/img-22f5b336-4142-40f9-90ad-a99eaefeea09.webp" width="200" alt="Unit Scales">
