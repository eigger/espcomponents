# 코콤 (Kocom) 월패드

[uartex](https://github.com/eigger/espcomponents/tree/latest/components/uartex) 기반 코콤 RS-485 월패드 패키지입니다.

| 파일 | 설명 |
|------|------|
| [`kocom.yaml`](kocom.yaml) | 범용 모놀리식 구성 (조명·난방·가스·엘리베이터 등 인라인 선언) |
| [`kocom_theart.yaml`](kocom_theart.yaml) | **Theart** 단지용 — `fragments/` 기반 선언형 구성 |
| [`kocom_thinks.yaml`](kocom_thinks.yaml) | **Thinks** 단지용 모놀리식 구성 (환기팬 포함) |
| [`kocom_door.yaml`](kocom_door.yaml) | 현관문 (도어락) |

하드웨어·핀맵은 상위 [`packages/wallpad/README.md`](../README.md)를 참고하세요.

---

## 기존 구성 방법 (모놀리식)

패키지 파일 하나에 `uart` / `uartex` / 엔티티가 모두 들어 있습니다. 단지 preset이 이미 맞으면 아래처럼 `packages:` remote include만 하면 됩니다.

### 월패드

```yaml
substitutions:
  friendly_name: "My Home"

packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents/
    files:
      - packages/wallpad/kocom/kocom.yaml        # 범용
      # - packages/wallpad/kocom/kocom_theart.yaml  # Theart (fragments)
      # - packages/wallpad/kocom/kocom_thinks.yaml  # Thinks
```

### 현관문

```yaml
packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents/
    files:
      - packages/wallpad/kocom/kocom_door.yaml
```

`kocom.yaml` / `kocom_thinks.yaml` 안의 `light:`, `climate:` 등 블록을 직접 복사·수정해 자신의 단지에 맞출 수도 있습니다.

---

## fragments 구성 방법 (Theart)

[`kocom_theart.yaml`](kocom_theart.yaml)은 공통 설정(uart, uartex, area/device)과 **방·가스·엘리베이터** 선언을 `fragments/`로 나눈 preset입니다.

### 전체 preset 사용

```yaml
substitutions:
  friendly_name: "My Home"

esphome:
  name: my-wallpad
  # areas / devices 는 kocom_theart.yaml 에 포함

esp32:
  board: m5stack-atom
  framework:
    type: arduino

packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents/
    files:
      - packages/wallpad/kocom/kocom_theart.yaml
```

### 방 단위 커스터마이즈

`kocom_theart.yaml`의 `packages:` 블록에서 방마다 [`fragments/room.yaml`](fragments/room.yaml) vars만 바꿉니다.

```yaml
packages:
  livingroom: !include
    file: fragments/room.yaml
    vars:
      room_num: 0          # 패킷 room 바이트 (0=거실, 1=안방, …)
      name_prefix: "Livingroom"
      device_id: device_livingroom
      light_count: 3       # 0~3 (조명 등 개수)
      aircon: 0            # 0=없음, 1=에어컨+온도센서 (스텁)
```

`room.yaml` 한 번 include로 아래가 붙습니다.

| 플랫폼 | fragment | 비고 |
|--------|----------|------|
| `light` | `lights_{light_count}.yaml` → `light.yaml` / `light_with_update.yaml` | 1번 등만 `command_update` |
| `climate` | `heater.yaml` | 난방 |
| `sensor` | `heater_temperature.yaml` | 실내 온도 |
| `climate` + `sensor` | `aircon_unit.yaml` | `aircon: 1` 일 때 (패킷 미확정, `disabled_by_default`) |

집 단위 공통 장치는 room 밖에서 따로 include 합니다.

```yaml
  gas: !include
    file: fragments/gas_package.yaml
    vars:
      device_id: device_kitchen

  elevator: !include
    file: fragments/elevator.yaml
    vars:
      device_id: device_entrance
```

### 개별 fragment 직접 사용

방 전체가 아니라 장치 하나만 쓸 때는 해당 fragment를 플랫폼 키 아래에 include 합니다.

```yaml
light:
  - !include
      file: packages/wallpad/kocom/fragments/light.yaml
      vars:
        room_num: 0
        light_num: 2
        light_name: "Livingroom Light 2"
        device_id: device_livingroom

packages:
  fan: !include
    file: packages/wallpad/kocom/fragments/fan_package.yaml
    vars:
      fan_name: "Navien Ventilator"
      device_id: device_kitchen
```

> **include vars 규칙:** fragment 파일 최상위는 `platform: uartex` 단일 dict이거나, `light:` / `climate:` 같은 플랫폼 키 아래 **리스트 항목**(`- !include`)으로 넣습니다. 리스트 `-` 없이 `light:` 아래에 바로 `!include`하면 schema 오류가 납니다.

### fragment vars 요약

| fragment | vars |
|----------|------|
| `room.yaml` | `room_num`, `name_prefix`, `device_id`, `light_count`, `aircon` |
| `light.yaml` / `light_with_update.yaml` | `room_num`, `light_num`, `light_name`, `device_id` |
| `heater.yaml` | `room_num`, `heater_name`, `device_id` |
| `heater_temperature.yaml` | `room_num`, `temp_name`, `device_id` |
| `gas.yaml` / `gas_package.yaml` | `device_id` |
| `elevator.yaml` | `device_id` |
| `fan.yaml` / `fan_package.yaml` | `fan_name`, `device_id` |

---

## 로컬 검증

CI에 연결되기 전, 아래 **validate** 파일로 `esphome config` 검증을 할 수 있습니다.

```bash
py -m esphome config packages/wallpad/kocom/validate_theart.yaml
py -m esphome config packages/wallpad/kocom/validate_aircon_stub.yaml
py -m esphome config packages/wallpad/kocom/validate_fan.yaml
```

| 파일 | 검증 대상 |
|------|-----------|
| `validate_theart.yaml` | `kocom_theart.yaml` 전체 |
| `validate_aircon_stub.yaml` | `room.yaml` + `aircon: 1` |
| `validate_fan.yaml` | 환기팬 fragment (`kocom_thinks` 계열) |

---

## 패킷 프로토콜

코콤 월패드 RS-485 프레임은 **고정 21바이트**입니다 (`uartex.rx_length: 21`).

### 프레임 레이아웃

```
 AA 55 | TYPE | DEV hi lo | ROOM | … | PAYLOAD (8B) | CHK | 0D 0D
 └─ header ─┘                              └─ footer ─┘
```

| 구간 | 바이트 | 설명 |
|------|--------|------|
| Header | 2 | `0xAA 0x55` (uartex `rx_header` / `tx_header`) |
| Type | 2 | `0x30 0xBC` 송신(TX) · `0x30 0xDC` 수신(RX) |
| Device | 2 | 장치 코드 (아래 표) |
| Room | 1 | 방 인덱스 (`room_num`: 0=거실, 1=안방, 2=방1, …) |
| Body | 8 | 고정 필드 + 명령·상태 (장치별 상이) |
| Payload | 8 | 장치별 데이터 (ESPHome **offset 8**부터) |
| Checksum | 1 | Header 제외 합산 mod 256 (`add_no_header`) |
| Footer | 2 | `0x0D 0x0D` |

> ESPHome `uartex` 설정의 **offset**은 checksum·footer 이전 전체 데이터 버퍼 기준이며, 일반적으로 **offset 8 = Payload 첫 바이트**입니다.

### Device 코드

| 코드 | 장치 |
|------|------|
| `0x00 0x01` | 월패드 |
| `0x00 0x0E` | 조명 |
| `0x00 0x2C` | 가스 |
| `0x00 0x33` | 현관문 |
| `0x00 0x36` | 난방(보일러) |
| `0x00 0x39` | 에어컨 |
| `0x00 0x3B` | 스위치 |
| `0x00 0x44` | 엘리베이터 |
| `0x00 0x48` | 환기팬 |
| `0x00 0x60` | 동작감지 |

### 자주 쓰는 Command

| 코드 | 의미 |
|------|------|
| `0x00 0x00` | 일반 제어 |
| `0x00 0x01` | 엘리베이터 |
| `0x00 0x02` | 가스 |
| `0x00 0x3A` | 상태 조회(Scan) |
| `0x00 0x98` | 센서 |

### Payload (offset 8~, 장치별)

#### 난방 `0x36`

| Offset | 내용 |
|--------|------|
| 8 | 전원 — `0x11` ON · `0x01`/`0x00` OFF (상위 니블 `0x10` = 가동) |
| 9 | 외출 — `0x01` Away · `0x00` None |
| 10 | 설정 온도 |
| 11 | 온수 온도 |
| 12 | 현재 온도 |
| 13 | 난방수 온도 |
| 14 | 에러 코드 |
| 15 | 보일러 에러 |

#### 에어컨 `0x39` (스텁, 패킷 미확정)

| Offset | 내용 |
|--------|------|
| 8 | 전원 — `0x10` ON · `0x00` OFF |
| 9 | 모드 — `0x00` 냉방 · `0x01` 송풍 · `0x02` 제습 · `0x03` 자동 |
| 10 | 풍량 — `0x00`~`0x03` |
| 11 | 현재 온도 |
| 12 | 설정 온도 |

#### 조명 `0x0E`

| Offset | 내용 |
|--------|------|
| 8~10 | 1~3번 등 — `0xFF` ON · `0x00` OFF |

다등 제어 시 해당 등만 `0xFF`/`0x00`으로 바꾸고 나머지는 `last_state`로 유지합니다.

#### 가스 `0x2C`

| Offset | 내용 |
|--------|------|
| 7 | 밸브 — `0x01` 열림(unlocked) · `0x02` 잠김(locked) |

#### 엘리베이터 `0x44`

| Offset | 내용 |
|--------|------|
| 8 | `0x00` 호출 · `0x03` 도착 |

#### 환기팬 `0x48`

| Offset | 내용 |
|--------|------|
| 8 | 전원 — `0x11` ON · `0x00` OFF |
| 10 | 풍량 — `0x40` 약 · `0x80` 중 · `0xC0` 강 |

### 패킷 예시

**조명 ON (거실, 1번 등)** — TX body:

```
30 BC  00 0E  00  01 00 00  FF 00 00  ...
      └ device ┘└room┘└─ payload ─┘
```

**가스 잠금** — TX body:

```
30 BC  00 2C  00  01 00 02  ...
```

**난방 RX** — 상태 수신 (`0x30 0xD0` …):

```
30 D0  00 36  {room}  …  offset 12 = 현재 온도
```

ACK는 Type을 `0x30 0xDC`로 바꾸고, 일부 장치는 Device·Room suffix를 붙입니다 (예: 조명 `… DC 00 0E {room}`).

---

## Theart preset 방 매핑 (기본값)

| package 키 | `room_num` | 조명 | 비고 |
|------------|------------|------|------|
| `livingroom` | 0 | 3 | 거실 |
| `bedroom` | 1 | 0 | 안방 |
| `room_1` | 2 | 0 | 방1 |
| `room_2` | 3 | 0 | 방2 |
| `livingroom_2` | 4 | 0 | 거실2 (난방만) |
| `gas` | — | — | 주방 `device_kitchen` |
| `elevator` | — | — | 현관 `device_entrance` |

단지마다 방 번호·등 개수가 다르면 위 표를 참고해 `room_num` / `light_count`만 조정하면 됩니다.
