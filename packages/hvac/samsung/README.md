# 삼성 시스템 에어컨 구성
```
packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents/
    files:
      - packages/hvac/samsung/samsung_system.yaml
```


# 삼성 시스템 에어컨 패킷 구조 (작성중)

- **Head**
  - Size: 1 byte
  - Description: Fixed value `0x32`

- **Checksum**
  - Size: 2 byte
  - Description: Validates data integrity
  - Calculation: CRC16-XMODEM

- **Footer**
  - Size: 2 bytes
  - Description: Fixed value `0x34`