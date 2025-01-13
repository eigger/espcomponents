# 코콤 패킷 구조

- **Head**
  - Size: 2 bytes
  - Description: Fixed value `0xAA, 0x55`

- **Type**
  - Size: 2 bytes
  - Description: Type of packet
  - Values:
    - `0x30, 0xBC`: Send
    - `0x30, 0xDC`: Receive

- **Device**
  - Size: 2 bytes
  - Description: Device identifier
  - Values:
    - `0x00, 0x01`: Wallpad
    - `0x00, 0x0E`: Light
    - `0x00, 0x2C`: Gas
    - `0x00, 0x33`: Door
    - `0x00, 0x36`: Heater
    - `0x00, 0x39`: Aircon
    - `0x00, 0x3B`: Switch
    - `0x00, 0x44`: Elevator
    - `0x00, 0x48`: Fan
    - `0x00, 0x60`: Motion

- **Room**
  - Size: 2 bytes
  - Description: Room identifier
  - Values:
    - `0x00, 0x01`: Livingroom
    - `0x01, 0x01`: Room1
    - `0x02, 0x01`: Room2
    - `0x03, 0x01`: Room3

- **Command**
  - Size: 2 bytes
  - Description: Command to execute
  - Values:
    - `0x00, 0x00`: Common
    - `0x00, 0x01`: Elevator
    - `0x00, 0x02`: Gas
    - `0x00, 0x04`: Motion
    - `0x00, 0x3A`: Scan
    - `0x00, 0x98`: Sensor

- **Payload**
  - Size: 8 bytes
  - Description: Data or additional information
  - Structure:
    - **Heater:**
      - Byte 0: Power state
        - `0x11`: Power on
        - `0x01`: Power off
      - Byte 1: Mode
        - `0x01`: Away
        - `0x00`: None
      - Byte 2: Target temperature
      - Byte 3: Hot water temperature
      - Byte 4: Current temperature
      - Byte 5: Heater water temperature
      - Byte 6: Error code
      - Byte 7: Boiler error
    - **Aircon:**
      - Byte 0: Power state
        - `0x10`: Power on
        - `0x00`: Power off
      - Byte 1: Mode
        - `0x00`: Cool
        - `0x01`: Fan only
        - `0x02`: Dry
        - `0x03`: Auto
      - Byte 2: Fan speed
        - `0x00`: Fan off
        - `0x01`: Fan low
        - `0x02`: Fan medium
        - `0x03`: Fan high
      - Byte 3: Current temperature
      - Byte 4: Target temperature
    - **Light & Switch:**
      - Byte 0: Light/Switch 1 state
        - `0xFF`: On
        - `0x00`: Off
      - Byte 1: Light/Switch 2 state
        - `0xFF`: On
        - `0x00`: Off
      - Byte 2: Light/Switch 3 state
        - `0xFF`: On
        - `0x00`: Off
    - **Fan:**
      - Byte 0: Onoff (CO2, 0x01)
        - `0x10`: On
        - `0x00`: Off
      - Byte 1: Fan mode
        - `0x00`: None
        - `0x01`: vent
        - `0x02`: auto
        - `0x03`: bypass
        - `0x05`: night
        - `0x08`: purifier
      - Byte 2: Fan Speed
        - `0x00`: Off
        - `0x40`: Low
        - `0x80`: Medium
        - `0xC0`: High
      - Byte 3, 4: Co2 value
      - Byte 5: Error Code
    - **Sensor:**
      - Byte 0: PM10
      - Byte 1: PM2.5
      - Byte 2, 3: Co2
      - Byte 4, 5: Voc
      - Byte 5: Temperature
      - Byte 6: Humidity
    - **Gas:**
      - None
    - **Elevator:**
      - Byte 0: Direction
        - `0x00`: None
        - `0x01`: Down
        - `0x02`: Up
        - `0x03`: Arrival
      - Byte 1: floor

- **Checksum**
  - Size: 1 byte
  - Description: Validates data integrity
  - Calculation: Sum of all field values modulo 256

- **Footer**
  - Size: 2 bytes
  - Description: Fixed value `0x0D, 0x0D`

## Examples

### Send Packet Example
Head: 0xAA, 0x55
Type: 0x30, 0xBC
Device: 0x00, 0x39 (Aircon)
Room: 0x00, 0x01 (Livingroom)
Command: 0x00, 0x98 (Sensor)
Payload: 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
Checksum: <sum of all fields modulo 256>
Footer: 0x0D, 0x0D

### Receive Packet Example
Head: 0xAA, 0x55
Type: 0x30, 0xDC
Device: 0x00, 0x2C (Gas)
Room: 0x01, 0x01 (Room1)
Command: 0x00, 0x02 (Gas)
Payload: 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
Checksum: <sum of all fields modulo 256>
Footer: 0x0D, 0x0D