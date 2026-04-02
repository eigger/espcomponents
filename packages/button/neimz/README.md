# Neimz Button

A simple button component configuration for ESP8266 (D1 Mini).

## Features

- Detects "Single Click" and "Hold" (Long Press).
- Sends events to Home Assistant (`esphome.esp_button`) to publish the button action (`action: "single"` or `action: "hold"`).
- Built-in LED feedback on D4 when the button is pressed.

## Wiring (ESP8266 D1 Mini)

| Button | ESP8266 (D1 Mini) |
|---|---|
| SIG / PIN | D2 (GPIO4) |
| GND | GND |

*Note: The pin is configured with an internal pull-up (`pullup: true`) and inverted logic (`inverted: true`). This means the button should connect the D2 pin to GND when pressed.*

## Images

| 1 | 2 | 3 |
|---|---|---|
| ![](../../../documents/button/1.jpg) | ![](../../../documents/button/2.jpg) | ![](../../../documents/button/3.jpg) |

## Purchase Link

[AliExpress](https://ko.aliexpress.com/item/1005006076558656.html)

## Home Assistant Automation Example

Here is an example automation that toggles a switch when a "Single Click" event is detected:

```yaml
alias: Button Single Click
description: ""
triggers:
  - trigger: event
    event_type: esphome.esp_button
    event_data:
      action: single
conditions: []
actions:
  - type: toggle
    device_id: d90a21ef3ca4d7bc156ec69dfd388ce1
    entity_id: ee4631bc9d12781978a5c5031c5cbd75
    domain: switch
mode: single
```