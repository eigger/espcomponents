## uartex
```
packet) 0x02 0x01 0x00 0x00 0x00 (add)checksum 0x0D 0x0A

uartex:
  rx_timeout: 10ms
  tx_delay: 50ms
  tx_timeout: 500ms
  tx_retry_cnt: 3

  rx_header: [0x02, 0x01] or "\x02\x01"
  rx_footer: [0x0D, 0x0A] or "\r\n"
  tx_header: [0x02, 0x01] or "\x02\x01"
  tx_footer: [0x0D, 0x0A] or "\r\n"

  rx_checksum: add
  tx_checksum: add

  version:
    disabled: False
  error:
    disabled: False
  log:
    disabled: False
```
### Configuration variables
- rx_timeout (Optional, Time): Data Receive Timeout. Defaults to 10ms. Max 2000ms
- tx_delay (Optional, Time): Data Send Delay. Defaults to 50ms. Max 2000ms
- tx_timeout (Optional, Time): ACK Reception Timeout. Defaults to 50ms. Max 2000ms
- tx_retry_cnt (Optional, int): Retry Count on ACK Failure. Defaults to 3. Max 10
- tx_ctrl_pin (Optional, gpio): Control PIN GPIO
- rx_header (Optional, array): Header of Data to be Received
- rx_footer (Optional, array): Footer of Data to be Received
- tx_header (Optional, array): Header of Data to be Transmitted
- tx_footer (Optional, array): Header of Data to be Transmitted
- rx_checksum (Optional, enum, lambda): Checksum of Data to be Received. (add, xor)
  - uint8_t = (uint8_t* data, uint16_t len)
- tx_checksum (Optional, enum, lambda): Checksum of Data to be Transmitted. (add, xor)
  - uint8_t = (uint8_t* data, uint16_t len)
- rx_checksum2 (Optional, enum or lambda): Checksum array of Data to be Received. (add, xor)
  - vector\<uint8_t\> = (uint8_t* data, uint16_t len)
- tx_checksum2 (Optional, enum or lambda): Checksum array of Data to be Transmitted. (add, xor)
  - vector\<uint8_t\> = (uint8_t* data, uint16_t len)
- on_read (Optional, lambda): Event of Data to be Received
  - void = (uint8_t* data, uint16_t len)
- on_write (Optional, lambda): Event of Data to be Transmitted
  - void = (uint8_t* data, uint16_t len)
- version (Optional): Version of Uartex
- error (Optional): Error of Uartex
- log (Optional): Log of Uartex
<hr/>

## State Schema
```
packet) 0x02 0x01 0x01 0x02 0x00 (add)checksum 0x0D 0x0A
offset) head head 0    1    2

state: 
  data: [0x01, 0x02] or "ascii string"
  mask: [0xff, 0xff] or "ascii string"
  offset: 0
  inverted: False
```
### Configuration variables
- data (Required, array or string): 
- mask (Optional, array or string): Defaults to []
- offset (Optional, int): Defaults to 0.
- inverted (Optional, bool): Defaults to False.
<hr/>

## Command Schema
```
packet) 0x02 0x01 0x01 0x02 0x01 (add)checksum 0x0D 0x0A
packet ack) 0x02 0x01 0xff 0x02 0x01 (add)checksum 0x0D 0x0A

excluding the header, checksum, and footer
command: 
  cmd: [0x01, 0x02, 0x01] or "ascii string"
  ack: [0xff] or "ascii string"
```
### Configuration variables
- cmd (Required, array or string): 
- ack (Optional, array or string): Defaults to []
<hr/>

## State Num Schema
```
packet) 0x02 0x01 0x00 0x01 0x02 (add)checksum 0x0D 0x0A
offset) head head 0    1    2

state_num: 
  offset: 2
  length: 1
  precision: 0

value = 0x02 
```
### Configuration variables
- offset (Required, int): (0 ~ 128)
- length (Optional, int): Defaults to 1. (1 ~ 4)
- precision (Optional, int): Defaults to 0. (0 ~ 5)
<hr/>

## uartex.light
```
packet on) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
   offset) head head 0    1    2
packet on ack) 0x02 0x01 0x02 0x13 0x01 (add)checksum 0x0D 0x0A
packet off) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A
packet off ack) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A

light:
  - platform: uartex
    name: "Room 0 Light 1"
    id: room_0_light_1
    state: 
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
    command_on:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13, 0x01]
    command_off: !lambda |-
      return {{0x02, 0x03, 0x00}, {0x02, 0x13, 0x00}};
```
### Configuration variables
- state (Required, state): 
- state_on (Required, state): 
- state_off (Required, state): 
- state_brightness (Optional, lambda):
  - float = (uint8_t* data, uint16_t len)
- command_on (Required, command or lambda): 
  - command = (void)
- command_off (Required, command or lambda): 
  - command = (void)
- command_brightness (Optional, lambda): 
  - command = (float x)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.binary_sensor
```
packet on) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
   offset) head head 0    1    2
packet off) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A

binary_sensor:
  - platform: uartex
    name: Binary_Sensor1
    state: [0x02, 0x03]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
```
### Configuration variables
- state (Required, state): 
- state_on (Required, state): 
- state_off (Required, state): 
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.button
```
packet on) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
   offset) head head 0    1    2

button:
  - platform: uartex
    name: "Elevator Call"
    icon: "mdi:elevator"
    command_on: 
      data: [0x02, 0x03, 0x01]
```
### Configuration variables
- command_on (Required, command or lambda): 
  - command = (void)
<hr/>

## uartex.climate
```
packet off) 0x02 0x01 0x02 0x03 0x00 target current (add)checksum 0x0D 0x0A
    offset) head head 0    1    2    3      4
packet off ack) 0x02 0x01 0x02 0x13 0x00 target current (add)checksum 0x0D 0x0A
packet heat) 0x02 0x01 0x02 0x03 0x01 target current (add)checksum 0x0D 0x0A
packet heat ack) 0x02 0x01 0x02 0x13 0x01 target current (add)checksum 0x0D 0x0A

climate:
  - platform: uartex
    name: "Room 0 Heater"
    id: room_0_heater
    visual:
      min_temperature: 5 °C
      max_temperature: 30 °C
      temperature_step: 1 °C
    state: 
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_temperature_current:
      offset: 4
      length: 1
      precision: 0
    state_temperature_target:
      offset: 3
      length: 1
      precision: 0
    state_off:
      offset: 2
      data: [0x00]
    state_heat:
      offset: 2
      data: [0x01]
    command_off:
      data: [0x02, 0x03, 0x00]
      ack: [0x02, 0x13, 0x00]
    command_heat: !lambda |-
      float target = id(room_0_heater).target_temperature;
      return {{0x02, 0x03, 0x01, (uint8_t)target, 0x00},{0x02, 0x13, 0x01}};
    command_temperature: !lambda |-
      // @param: const float x
      float target = x;
      return {{0x02, 0x03, 0x01, (uint8_t)target, 0x00},{0x02, 0x13, 0x01}};
```
### Configuration variables
- state (Required, state): 
- state_off (Required, state): 
- state_temperature_current (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- state_temperature_target (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- state_humidity_current (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- state_humidity_target (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- state_cool (Optional, state): 
- state_heat (Optional, state): 
- state_fan_only (Optional, state): 
- state_dry (Optional, state): 
- state_auto (Optional, state): 
- state_swing_off (Optional, state): 
- state_swing_both (Optional, state): 
- state_swing_vertical (Optional, state): 
- state_swing_horizontal (Optional, state): 
- state_fan_on (Optional, state): 
- state_fan_off (Optional, state): 
- state_fan_auto (Optional, state): 
- state_fan_low (Optional, state): 
- state_fan_medium (Optional, state): 
- state_fan_high (Optional, state): 
- state_fan_middle (Optional, state): 
- state_fan_focus (Optional, state): 
- state_fan_diffuse (Optional, state): 
- state_fan_quiet (Optional, state): 
- state_preset_none (Optional, state): 
- state_preset_home (Optional, state): 
- state_preset_away (Optional, state): 
- state_preset_boost (Optional, state): 
- state_preset_comfort (Optional, state): 
- state_preset_eco (Optional, state): 
- state_preset_sleep (Optional, state): 
- state_preset_activity (Optional, state): 
- state_custom_fan (Optional, lambda): 
  - std::string = (uint8_t* data, uint16_t len)
- state_custom_preset (Optional, lambda): 
  - std::string = (uint8_t* data, uint16_t len)
- command_off (Optional, command or lambda): 
  - command = (void)
- command_temperature (Optional, lambda): 
  - command = (float x)
- command_humidity (Optional, lambda): 
  - command = (float x)
- command_cool (Optional, command or lambda): 
  - command = (void)
- command_heat (Optional, command or lambda): 
  - command = (void)
- command_fan_only (Optional, command or lambda): 
  - command = (void)
- command_dry (Optional, command or lambda): 
  - command = (void)
- command_auto (Optional, command or lambda): 
  - command = (void)
- command_swing_off (Optional, command or lambda): 
  - command = (void)
- command_swing_both (Optional, command or lambda): 
  - command = (void)
- command_swing_vertical (Optional, command or lambda): 
  - command = (void)
- command_swing_horizontal (Optional, command or lambda): 
  - command = (void)
- command_fan_on (Optional, command or lambda): 
  - command = (void)
- command_fan_off (Optional, command or lambda): 
  - command = (void)
- command_fan_auto (Optional, command or lambda): 
  - command = (void)
- command_fan_low (Optional, command or lambda): 
  - command = (void)
- command_fan_medium (Optional, command or lambda): 
  - command = (void)
- command_fan_high (Optional, command or lambda): 
  - command = (void)
- command_fan_middle (Optional, command or lambda): 
  - command = (void)
- command_fan_focus (Optional, command or lambda): 
  - command = (void)
- command_fan_diffuse (Optional, command or lambda): 
  - command = (void)
- command_fan_quiet (Optional, command or lambda): 
  - command = (void)
- command_preset_none (Optional, command or lambda): 
  - command = (void)
- command_preset_away (Optional, command or lambda): 
  - command = (void)
- command_preset_boost (Optional, command or lambda): 
  - command = (void)
- command_preset_comfort (Optional, command or lambda): 
  - command = (void)
- command_preset_eco (Optional, command or lambda): 
  - command = (void)
- command_preset_sleep (Optional, command or lambda): 
  - command = (void)
- command_preset_activity (Optional, command or lambda): 
  - command = (void)
- command_update (Optional, command or lambda): 
  - command = (void)
- command_custom_fan (Required, lambda): 
  - command = (std::string str)
- command_custom_preset (Required, lambda): 
  - command = (std::string str)
- custom_fan_mode (Optional, list): A list of custom fan mode for this climate
- custom_preset (Optional, list): A list of custom preset mode for this climate
<hr/>

## uartex.fan
```
packet off) 0x02 0x01 0x02 0x03 0x00 speed (add)checksum 0x0D 0x0A
    offset) head head 0    1    2    3
packet off ack) 0x02 0x01 0x02 0x13 0x00 speed (add)checksum 0x0D 0x0A
packet on) 0x02 0x01 0x02 0x03 0x01 speed (add)checksum 0x0D 0x0A
packet on ack) 0x02 0x01 0x02 0x13 0x01 speed (add)checksum 0x0D 0x0A

fan:
  - platform: uartex
    name: "Fan1"
    state:
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
    command_on:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13]
    command_off:
      data: [0x02, 0x03, 0x00]
      ack: [0x02, 0x13]
    command_speed: !lambda |-
      // @param: const float x
      return {
                {0x02, 0x03, 0x01, (uint8_t)x},
                {0x02, 0x13}
              };
    state_speed: !lambda |-
      // @param: const uint8_t *data, const unsigned short len
      // @return: const float
      {
        return data[3];
      }
```
### Configuration variables
- state (Required, state): 
- state_on (Required, state): 
- state_off (Required, state): 
- state_speed (Optional, lambda):
  - float = (uint8_t* data, uint16_t len)
- state_preset (Optional, lambda): 
  - std::string = (uint8_t* data, uint16_t len)
- command_on (Required, command or lambda): 
  - command = (void)
- command_off (Required, command or lambda): 
  - command = (void)
- command_speed (Optional, lambda): 
  - command = (float x)
- command_preset (Required, lambda): 
  - command = (std::string str)
- command_update (Optional, command or lambda): 
  - command = (void)
- preset_modes (Optional, list): A list of preset modes for this fan
<hr/>

## uartex.lock
```
packet unlock) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A
       offset) head head 0    1    2
packet unlock ack) 0x02 0x01 0x02 0x13 0x00 (add)checksum 0x0D 0x0A
packet lock) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
packet lock ack) 0x02 0x01 0x02 0x13 0x01 (add)checksum 0x0D 0x0A

lock:
  - platform: uartex
    name: "Lock1"
    state:
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_locked:
      offset: 2
      data: [0x01]
    state_unlocked:
      offset: 2
      data: [0x00]
    state_locking:
      offset: 2
      data: [0x02]
    state_unlocking:
      offset: 2
      data: [0x03]
    state_jammed:
      offset: 2
      data: [0x04]
    command_lock:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13]
    command_unlock:
      data: [0x02, 0x03, 0x00]
      ack: [0x02, 0x13]
```
### Configuration variables
- state (Required, state): 
- state_locked (Optional, state): 
- state_unlocked (Optional, state): 
- state_locking (Optional, state): 
- state_unlocking (Optional, state): 
- state_jammed (Optional, state): 
- command_lock (Optional, command or lambda): 
  - command = (void)
- command_unlock (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.number
```
packet) 0x02 0x01 0x02 0x03 0x00 number (add)checksum 0x0D 0x0A
offset) head head 0    1    2    3
packet ack) 0x02 0x01 0x02 0x13 0x00 number (add)checksum 0x0D 0x0A

number:
  - platform: uartex
    name: "Number1"
    state:
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    max_value: 10
    min_value: 1
    step: 1
    state_number:
      offset: 3
      length: 1
      precision: 0
    command_number: !lambda |-
      // @param: const float x
      return {
                {0x02, 0x03, 0x00, (uint8_t)x},
                {0x02, 0x13}
              };
```
### Configuration variables
- state (Required, state): 
- state_number (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- command_number (Optional, lambda): 
  - command = (float x)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.sensor
```
packet) 0x02 0x01 0x02 0x03 0x00 value (add)checksum 0x0D 0x0A
offset) head head 0    1    2    3
sensor:
  - platform: uartex
    name: Sensor1
    state: [0x02, 0x03, 0x00]
    state_number:
      offset: 3
      length: 1
      precision: 0
```
### Configuration variables
- state (Required, state): 
- state_number (Optional, state_num or lambda): 
  - float = (uint8_t* data, uint16_t len)
- lambda (Optional, lambda): 
  - float = (uint8_t* data, uint16_t len)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.switch
```
packet on) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
   offset) head head 0    1    2
packet on ack) 0x02 0x01 0x02 0x13 0x01 (add)checksum 0x0D 0x0A
packet off) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A
packet off ack) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A

switch:
  - platform: uartex
    name: "Switch1"
    state: 
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
    command_on:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13, 0x01]
    command_off: !lambda |-
      return {{0x02, 0x03, 0x00}, {0x02, 0x13, 0x00}};
```
### Configuration variables
- state (Required, state): 
- state_on (Required, state): 
- state_off (Required, state): 
- command_on (Required, command or lambda): 
  - command = (void)
- command_off (Required, command or lambda): 
  - command = (void)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.text
```
text:
  - platform: uartex
    name: "Text"
    command_text: !lambda |-
      return {
                {0x0F, 0x01, 0x01},
                {0x0F, 0x01}
              };
```
### Configuration variables
- command_text (Required, lambda): 
  - command = (std::string str)
<hr/>

## uartex.text_sensor
```
packet on) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
   offset) head head 0    1    2
packet off) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A

text_sensor:
  - platform: uartex
    name: "Text Sensor"
    state: [0x02, 0x03]
    lambda: |-
      {
        if (data[2] == 0x01) return "ON";
        else return "OFF";
      }
```
### Configuration variables
- state (Required, state): 
- lambda (Optional, lambda): 
  - std::string = (uint8_t* data, uint16_t len)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.valve
```
packet open) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
offset) head head 0    1    2    3
packet open ack) 0x02 0x01 0x02 0x13 0x01 (add)checksum 0x0D 0x0A
packet close) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A
packet close ack) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A

valve:
  - platform: uartex
    name: "Valve1"
    state: 
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_open:
      offset: 2
      data: [0x01]
    state_closed:
      offset: 2
      data: [0x00]
    command_open:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13, 0x01]
    command_close:
      data: [0x02, 0x03, 0x00]
      ack: [0x02, 0x13, 0x00]
```
### Configuration variables
- state (Required, state): 
- state_open (Optional, state): 
- state_closed (Optional, state): 
- state_position (Optional, lambda):
  - float = (uint8_t* data, uint16_t len)
- command_open (Optional, command or lambda): 
  - command = (void)
- command_close (Optional, command or lambda): 
  - command = (void)
- command_stop (Optional, command or lambda): 
  - command = (void)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>
