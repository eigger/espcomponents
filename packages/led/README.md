# Puppy LED Lamp

ESPHome configuration for a small dog-shaped LED lamp based on the Wemos D1 Mini (ESP8266). This package provides a simple monochromatic light with PWM dimming control.

## Where to Buy

- [AliExpress - Mini Puppy Table Lamp](https://ko.aliexpress.com/item/1005011925580834.html)

## Features

- **PWM Dimming**: Smooth brightness control for the LED.
- **Smart Integration**: Easily control the lamp via Home Assistant or the ESPHome web interface.

## Configuration Usage

Add the following to your ESPHome configuration:

```yaml
substitutions:
  name: "puppy-led"
  friendly_name: "Puppy LED"

packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents/
    files:
      - packages/led/puppy_d1_mini.yaml
```

## Hardware Configuration

### Board
- **Board**: `d1_mini` (ESP8266)

### Wiring

- **Pin D6 (GPIO12)** <-> Resistor <-> LED (+)
- **GND** <-> LED (-)

## Images

| 1 | 2 | 3 |
|---|---|---|
| ![Puppy Lamp 1](../../documents/led/2.jpg) | ![Puppy Lamp 2](../../documents/led/3.jpg) | ![Puppy Lamp 3](../../documents/led/4.jpg) |

| 4 | 5 |
|---|---|
| ![Puppy Lamp 4](../../documents/led/5.jpg) | ![Puppy Lamp 5](../../documents/led/1.gif) |

