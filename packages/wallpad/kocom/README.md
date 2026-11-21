# 코콤 (Kocom) 월패드

[uartex](../../../components/uartex) 기반 코콤 RS-485 월패드 패키지입니다.
처음 설치한다면 위키의 [월패드 시작하기](https://github.com/eigger/espcomponents/wiki/ESPHome-%EC%9B%94%ED%8C%A8%EB%93%9C-%EC%8B%9C%EC%9E%91%ED%95%98%EA%B8%B0)를 따라 하세요.

[파일](#파일) · [사용하기](#사용하기) · [Theart 방 구성 바꾸기](#theart-방-구성-바꾸기) · [패킷 프로토콜](#패킷-프로토콜) · [제보하기](#제보하기) · [로컬 검증](#로컬-검증)

## 파일

| 파일 | 대상 | 구성 방식 |
| --- | --- | --- |
| [`kocom.yaml`](kocom.yaml) | 범용 — 조명·난방·가스·엘리베이터 | 한 파일에 모든 엔티티 |
| [`kocom_thinks.yaml`](kocom_thinks.yaml) | **Thinks** 단지 — 범용 + 환기팬 | 한 파일에 모든 엔티티 |
| [`kocom_theart.yaml`](kocom_theart.yaml) | **Theart** 단지 | [`fragments/`](fragments)를 방 단위로 조립 |
| [`kocom_door.yaml`](kocom_door.yaml) | 현관문 (도어락) | 별도 장치용 |

모든 파일의 기본 UART는 **9600 / 8N1**, RX `GPIO22` / TX `GPIO19`(Atom Lite + Atomic RS485 Base)입니다.
다른 보드·모듈은 [하드웨어와 UART 핀](../README.md#하드웨어와-uart-핀)을 참고해 `uart:` 핀을 덮어쓰세요.

## 사용하기

### 월패드

`files:`의 마지막 줄에 단지에 맞는 파일 하나를 넣습니다.

```yaml
substitutions:
  name: "esp-kocom"
  friendly_name: "ESP Kocom"

esp32:
  board: m5stack-atom
  framework:
    type: esp-idf

packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: latest
    files:
      - packages/device_base.yaml
      - packages/wallpad/kocom/kocom.yaml           # 범용
      # - packages/wallpad/kocom/kocom_thinks.yaml  # Thinks
      # - packages/wallpad/kocom/kocom_theart.yaml  # Theart
```

### 현관문

현관문은 월패드와 다른 RS-485 라인을 사용하므로 **장치를 따로 만듭니다.**
같은 장치에 월패드 파일과 함께 넣으면 `uart`/`uartex`가 겹칩니다.

```yaml
substitutions:
  name: "esp-kocom-door"
  friendly_name: "ESP Kocom Door"

esp32:
  board: m5stack-atom
  framework:
    type: esp-idf

packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: latest
    files:
      - packages/device_base.yaml
      - packages/wallpad/kocom/kocom_door.yaml
```

### 수정하기

| 하고 싶은 것 | 방법 |
| --- | --- |
| 엔티티 이름 변경, 안 쓰는 방 삭제 | `!extend` / `!remove` — [위키: 월패드 응용하기 3-1·3-2](https://github.com/eigger/espcomponents/wiki/ESPHome-%EC%9B%94%ED%8C%A8%EB%93%9C-%EC%9D%91%EC%9A%A9%ED%95%98%EA%B8%B0#3-yaml로-수정하기) |
| 패킷·조명 개수 직접 수정 (범용·Thinks) | 파일 내용을 복사해서 수정 — [위키: 응용하기 3-4](https://github.com/eigger/espcomponents/wiki/ESPHome-%EC%9B%94%ED%8C%A8%EB%93%9C-%EC%9D%91%EC%9A%A9%ED%95%98%EA%B8%B0#3-4-월패드-yaml-전체를-가져와서-고치기) |
| Theart 방·조명 구성 변경 | 아래 [Theart 방 구성 바꾸기](#theart-방-구성-바꾸기) |

> [!NOTE]
> 코콤 조명 명령은 같은 방의 다른 등 상태를 마지막으로 수신한 패킷에서 유지합니다. 개별 엔티티를 삭제해도 나머지 조명은 해당 상태 바이트를 계속 유지합니다.

## Theart 방 구성 바꾸기

[`kocom_theart.yaml`](kocom_theart.yaml)은 공통 설정(uart, uartex, area/device) 아래에 방·가스·엘리베이터를 [`fragments/`](fragments)로 조립합니다.

### 기본 방 구성

| 키 | `room_num` | 조명 | Device | 비고 |
| --- | --- | --- | --- | --- |
| `livingroom` | 0 | 3 | `device_livingroom` | 거실 |
| `bedroom` | 1 | 0 | `device_bedroom` | 안방 |
| `room_1` | 2 | 0 | `device_room_1` | 방1 |
| `room_2` | 3 | 0 | `device_room_2` | 방2 |
| `livingroom_2` | 4 | 0 | `device_livingroom` | 거실2 (난방만) |
| `gas` | — | — | `device_kitchen` | 가스 |
| `elevator` | — | — | `device_entrance` | 엘리베이터 |

### 같은 방 조명 상태 유지하기

각 조명 명령은 마지막으로 수신한 같은 방의 전체 조명 상태에서 다른 등 바이트를 유지합니다. 전체 상태를 수신하기 전과 명령을 보낸 후 새로운 상태가 올 때까지 다음 명령은 대기합니다. 상태 필터는 조명에서 월패드로 향하는 패킷 중 명령 바이트가 `00`이고 세 조명 바이트를 포함한 패킷을 받습니다. `30 DC`도 전체 상태 패킷이면 수신합니다. 패킷 방향과 명령 바이트는 [Kocom RS485 파서](https://github.com/zooil/kocomRS485/blob/master/rs485.py)를 참고했습니다.

범용·Thinks·Theart 패키지는 `preserve_state`를 지원하는 UARTEx `latest`를 기본으로 사용합니다. `kocom_uartex_ref` substitution으로 지원 버전을 지정할 수 있습니다.

### 우리 집에 맞게 바꾸기

GitHub의 fragment에 `vars`를 넘겨서 방 단위로 조립합니다.

1. [`kocom_theart.yaml`](kocom_theart.yaml)의 **처음부터 `packages:` 직전까지**(uart, external_components, uartex, esphome areas/devices)를 내 YAML에 복사합니다.
2. 아래처럼 `device_base.yaml`과 방 fragment를 원격 패키지로 추가하고 `vars`를 우리 집에 맞게 바꿉니다.

```yaml
packages:
  base:
    url: https://github.com/eigger/espcomponents
    ref: latest
    files:
      - packages/device_base.yaml
  rooms:
    url: https://github.com/eigger/espcomponents
    ref: latest
    files:
      - path: packages/wallpad/kocom/fragments/room.yaml
        vars:
          room_num: 0               # 패킷의 방 번호 (0=거실, 1=안방, …)
          name_prefix: "Livingroom"
          device_id: device_livingroom
          light_count: 2            # 조명 개수 0~3
          aircon: 0                 # 0=없음, 1=에어컨 (패킷 미확정)
      - path: packages/wallpad/kocom/fragments/room.yaml
        vars:
          room_num: 1
          name_prefix: "Bedroom"
          device_id: device_bedroom
          light_count: 1
          aircon: 0
      - path: packages/wallpad/kocom/fragments/gas_package.yaml
        vars:
          device_id: device_kitchen
      - path: packages/wallpad/kocom/fragments/elevator.yaml
        vars:
          device_id: device_entrance
      - path: packages/wallpad/kocom/fragments/fan_package.yaml   # 환기팬이 있다면
        vars:
          fan_name: "Ventilator"
          device_id: device_kitchen
```

### `room.yaml`이 만드는 엔티티

| 플랫폼 | fragment | 비고 |
| --- | --- | --- |
| `light` | `lights_{light_count}.yaml` → `light.yaml` / `light_with_update.yaml` | 1번 등만 `command_update` |
| `climate` | `heater.yaml` | 난방 |
| `sensor` | `heater_temperature.yaml` | 실내 온도 |
| `climate` + `sensor` | `aircon_unit.yaml` | `aircon: 1`일 때 — 패킷 미확정이라 `disabled_by_default` |

### fragment별 vars

| fragment | vars |
| --- | --- |
| `room.yaml` | `room_num`, `name_prefix`, `device_id`, `light_count`, `aircon` |
| `gas_package.yaml` | `device_id` |
| `elevator.yaml` | `device_id` |
| `fan_package.yaml` | `fan_name`, `device_id` |
| `light.yaml` / `light_with_update.yaml` | `room_num`, `light_num`, `light_name`, `device_id` |
| `heater.yaml` | `room_num`, `heater_name`, `device_id` |
| `heater_temperature.yaml` | `room_num`, `temp_name`, `device_id` |

> [!NOTE]
> `packages:`로 넣을 수 있는 것은 최상위가 `light:`·`climate:` 같은 플랫폼 키인 파일(`room.yaml`, `*_package.yaml`, `elevator.yaml`)입니다.
> `light.yaml`, `heater.yaml`처럼 엔티티 하나만 있는 fragment는 로컬에 저장소를 받아 `light:` 목록 항목으로 `- !include` 해야 합니다.

## 패킷 프로토콜

코콤 RS-485 프레임은 **21바이트 고정**입니다 (`uartex.rx_length: 21`).

```
AA 55 | 30 BC | 00 | 0E 03 | 01 00 | 00 | FF 00 00 00 00 00 00 00 | CHK | 0D 0D
헤더    타입         장치·방    상대 장치·방  명령  데이터 (8)                      체크섬  푸터
```

uartex의 `offset`은 헤더를 뺀 데이터 기준입니다.

| offset | 크기 | 내용 |
| --- | --- | --- |
| 0–1 | 2 | 타입 — `30 BC` 명령 · `30 D0`~`30 DF` 상태 · `30 DC` 응답(ACK) |
| 2 | 1 | `00` |
| 3 | 1 | 장치 코드 (아래 표) |
| 4 | 1 | 방 번호 (`room_num`) |
| 5 | 1 | 상대 장치 코드 (대부분 월패드 `01`) |
| 6 | 1 | 상대 방 번호 |
| 7 | 1 | 명령 (아래 표) |
| 8–15 | 8 | 데이터 — 장치별 |

체크섬은 헤더를 뺀 모든 바이트의 합 & `0xFF`(`add_no_header`), 푸터는 `0D 0D`입니다.
엘리베이터처럼 offset 3에 월패드(`01`), offset 5에 장치(`44`)가 오는 경우도 있으니 정확한 값은 YAML과 수신 로그를 기준으로 확인하세요.

### 장치 코드

| 코드 | 장치 | 코드 | 장치 |
| --- | --- | --- | --- |
| `01` | 월패드 | `39` | 에어컨 |
| `0E` | 조명 | `3B` | 스위치 |
| `2C` | 가스 | `44` | 엘리베이터 |
| `33` | 현관문 | `48` | 환기팬 |
| `36` | 난방 | `60` | 동작감지 |

### 명령

| 코드 | 의미 |
| --- | --- |
| `00` | 일반 제어 |
| `01` | 엘리베이터 |
| `02` | 가스 |
| `04` | 동작감지 |
| `3A` | 상태 조회 |
| `98` | 센서 |

### 데이터 (offset 8~15)

| 장치 | offset | 내용 |
| --- | --- | --- |
| 조명 `0E` | 8~10 | 1~3번 등 — `FF` 켜짐 · `00` 꺼짐 |
| 난방 `36` | 8 | 전원 — `11` 켜짐 · `01`/`00` 꺼짐 |
| | 9 | 외출 — `01` 외출 · `00` 해제 |
| | 10 / 11 / 12 / 13 | 설정 온도 / 온수 온도 / 현재 온도 / 난방수 온도 |
| | 14 / 15 | 에러 코드 / 보일러 에러 |
| 가스 `2C` | 7 (명령) | `01` 열림 · `02` 잠김 |
| 엘리베이터 `44` | 8 | 상태 — `00` 정지 · `01` 하강 · `02` 상승 · `03` 도착 (호출 명령은 `03`) |
| | 9 | 현재 층 |
| 환기팬 `48` | 8 | 전원 — `11` 켜짐 · `00` 꺼짐 |
| | 10 | 풍량 — `40` 약 · `80` 중 · `C0` 강 |
| 에어컨 `39` (미확정) | 8 / 9 / 10 | 전원(`10` 켜짐) / 모드(`00` 냉방 `01` 송풍 `02` 제습 `03` 자동) / 풍량(`00`~`03`) |
| | 11 / 12 | 현재 온도 / 설정 온도 |

조명은 한 번에 방 전체(1~3번 등)의 상태를 보내므로, 제어할 등만 바꾸고 나머지 등은 현재 상태를 그대로 넣습니다.

### 예시

```
조명 켜기 (방 3, 1번 등):  30 BC 00 0E 03 01 00 00  FF 00 00 00 00 00 00 00
가스 잠금:                 30 BC 00 2C 00 02 00 00  00 00 00 00 00 00 00 00
엘리베이터 호출:           30 BC 00 01 00 44 00 01  03 00 00 00 00 00 00 00
```

## 제보하기

구성이 **잘 동작했거나**, 새 단지·장치 **구성을 추가했거나**, **오류가 발생했다면** 제보해 주세요. 같은 월패드를 쓰는 다른 분들께 큰 도움이 됩니다.

- [이슈 등록](https://github.com/eigger/espcomponents/issues/new) — 동작 확인, 오류, 구성 추가 (Pull Request도 환영합니다)
- [질문·토론](https://github.com/eigger/espcomponents/discussions/new?category=q-a)

제보할 때 **제조사·단지(모델)**, **사용한 파일**, 가능하면 **수신 로그**를 함께 적어 주시면 확인이 빠릅니다.

## 로컬 검증

저장소를 받은 상태에서 fragment 구성을 `esphome config`로 확인할 수 있습니다.

```bash
esphome config tests/packages/wallpad/kocom/validate_theart.yaml
esphome config tests/packages/wallpad/kocom/validate_aircon_stub.yaml
esphome config tests/packages/wallpad/kocom/validate_fan.yaml
```

| 파일 | 검증 대상 |
| --- | --- |
| `validate_theart.yaml` | `kocom_theart.yaml` 전체 |
| `validate_aircon_stub.yaml` | `room.yaml` + `aircon: 1` |
| `validate_fan.yaml` | 환기팬 fragment |
