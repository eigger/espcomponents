### add esphome *.yaml
<details>
    <summary>예제</summary>

  ```

external_components:
  - source:
      type: git
      url: https://github.com/eigger/espcomponents/
    components: [ uartex ]
    refresh: always

uart:
  baud_rate: 9600
  data_bits: 8
  parity: NONE
  stop_bits: 1
  rx_pin: GPIO32
  tx_pin: GPIO26
  debug:

uartex:
  rx_timeout: 10ms    #해당시간안에 수신되는 데이터를 하나의 데이터로 처리
  tx_delay: 50ms      #데이터 수신후 설정된 시간후 명령 전송
  tx_timeout: 50ms    #명렁 전송후 ACK 응답까지 대기 시간
  tx_retry_cnt: 3     #ACK수신까지 명려어 재전송
  
  rx_header: [0xFE]   #수신 시작문자
  rx_footer: [0xEE]   #수신 끝문자
  tx_header: [0xFE]   #송신 시작문자
  tx_footer: [0xEE]   #송신 끝문자
  #rx_checksum: add
  #rx_checksum: !lambda |-
  #  // @param: const uint8_t *data, const unsigned short len
  #  // @return: uint8_t
  #  uint8_t crc = 0x00;
  #  for(num_t i=0; i<len; i++)
  #    crc += data[i];
  #  return crc;
    
  #tx_checksum: add
  #tx_checksum: !lambda |-
  #  // @param: const uint8_t *data, const unsigned short len
  #  // @return: uint8_t
  #  uint8_t crc = 0x00;
  #  for(num_t i=0; i<len; i++)
  #    crc += data[i];
  #  return crc;
  
# 0xFE 0x06 0x01 0x?? 0x?? ack e=0xEE  
text_sensor:
  - platform: uartex
    name: TextSensor1
    filter: [0x06, 0x01]
    lambda: |-
      // @param: const uint8_t *data, const unsigned short len
      // @return: const char*
      {
        String str = "데이터: ";
        for(int i = 0; i < len; i++)
        {
          str += String(data[i], HEX);
        }
        return str.c_str();
      }

  
# offset                0    1    2    3    
# state_on:     0xFE 0x07 0x01 0x01 0x01  ack 0xEE
# state_off:    0xFE 0x07 0x01 0x00 0x01  ack 0xEE
# speed low:    0xFE 0x07 0x01 0x01 0x01  ack 0xEE
# speed mid:    0xFE 0x07 0x01 0x01 0x02  ack 0xEE
# speed high:   0xFE 0x07 0x01 0x01 0x03  ack 0xEE
fan:
  - platform: uartex
    name: "Fan1"
    filter: [0x07, 0x01]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
    command_on:
      data: [0x07, 0x01, 0x01]
      ack: [0x07, 0x01]
    command_off:
      data: [0x07, 0x01, 0x00]
      ack: [0x07, 0x01]
    command_speed: !lambda |-
      // @param: const float x
      return {
                {0x07, 0x01, 0x01, (uint8_t)x},
                {0x07, 0x01}
             };
    state_speed: !lambda |-
      // @param: const uint8_t *data, const unsigned short len
      // @return: const float
      {
        return data[3];
      }



# offset                0    1    2        
# state_on:     0xFE 0x08 0x01 0x01  ack 0xEE
# state_off:    0xFE 0x08 0x01 0x00  ack 0xEE
switch:
  - platform: uartex
    name: "Switch1"
    filter: [0x08, 0x01]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
    command_on:
      data: [0x08, 0x01, 0x01]
      ack: [0x08, 0x01]
    command_off:
      data: [0x08, 0x01, 0x00]
      ack: [0x08, 0x01]
  
# offset                0    1    2        
# state_on:     0xFE 0x09 0x01 0x01  ack 0xEE
# state_off:    0xFE 0x09 0x01 0x00  ack 0xEE  
binary_sensor:
  - platform: uartex
    name: Binary_Sensor1
    filter: [0x09, 0x01]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]

# offset                    0    1    2        
# state_number:     0xFE 0x08 0x01 0x0A  ack 0xEE
#                                  = 10
sensor:
  - platform: uartex
    name: Sensor1
    filter: [0x0A, 0x01]
    state_number:
      offset: 2
      length: 1
      precision: 0

# offset                0    1    2        
# state_on:     0xFE 0x0B 0x01 0x01  ack 0xEE
# state_off:    0xFE 0x0B 0x01 0x00  ack 0xEE  
light:
  - platform: uartex
    name: "Light1"
    filter: [0x0B, 0x01]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
    command_on:
      data: [0x0B, 0x01, 0x01]
      ack: [0x0B, 0x01]
    command_off:
      data: [0x0B, 0x01, 0x00]
      ack: [0x0B, 0x01]

climate:
  - platform: uartex
    name: "Climate2"
    visual:
      min_temperature: 5 °C
      max_temperature: 30 °C
      temperature_step: 1 °C
    filter: [0x0C, 0x01]
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
      data: [0x01]
    state_cool:
      offset: 2
      data: [0x00]
    command_off: 
      data: [0x0C, 0x01, 0x00]
      ack: [0x0C, 0x01]
    command_cool:
      data: [0x0C, 0x01, 0x01]
      ack: [0x0C, 0x01]
    command_temperature: !lambda |-
      // @param: const float x
      return {
                {0x0C, 0x01, 0x01, (uint8_t)x},
                {0x0C, 0x01}
             };
  - platform: uartex
    name: "Climate1"
    visual:
      min_temperature: 5 °C
      max_temperature: 30 °C
      temperature_step: 1 °C
    filter: [0x0C, 0x01]
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
      data: [0x01]
    state_heat:
      offset: 2
      data: [0x00]
    command_off: 
      data: [0x0C, 0x01, 0x00]
      ack: [0x0C, 0x01]
    command_heat:
      data: [0x0C, 0x01, 0x01]
      ack: [0x0C, 0x01]
    command_temperature: !lambda |-
      // @param: const float x
      return {
                {0x0C, 0x01, 0x01, (uint8_t)x},
                {0x0C, 0x01}
             };

button:
  - platform: uartex
    name: "Button1"
    icon: "mdi:elevator"
    command_on: 
      data: [0x0D, 0x01, 0x01]
      ack: [0x0D, 0x01]

lock:
  - platform: uartex
    name: "Lock1"
    filter: [0x0E, 0x01]
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
      data: [0x0E, 0x01, 0x01]
      ack: [0x0E, 0x01]
    command_unlock:
      data: [0x0E, 0x01, 0x00]
      ack: [0x0E, 0x01]

number:
  - platform: uartex
    name: "Number1"
    filter: [0x0F, 0x01]
    max_value: 10
    min_value: 1
    step: 1
    state_number:
      offset: 2
      length: 1
      precision: 0
    command_number: !lambda |-
      // @param: const float x
      return {
                {0x0F, 0x01, 0x01, (uint8_t)x},
                {0x0F, 0x01}
             };
```
</details>

