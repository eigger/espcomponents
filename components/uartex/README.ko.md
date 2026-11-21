# UARTEx 한국어 사용 안내

UARTEx는 UART 또는 RS-485로 주고받는 장치의 패킷을 ESPHome 엔티티에 연결하는 컴포넌트입니다. 장치의 전송 속도와 프레임 형식이 필요합니다. 월패드를 연결한다면 제조사별 프리셋에서 같은 모델의 설정을 먼저 확인하세요.

[English README](README.md) · [AI 에이전트용 패킷 구성 지침](PACKET_GUIDE.ko.md) · [월패드 프리셋](../../packages/wallpad) · [TCP 포워딩](../tcp_server)

## 문서 기준

2026-11-21에 확인한 espcomponents 커밋 `e3b1d72`의 UARTEx를 기준으로 작성했습니다. 아래 스위치·채널 상태·온도 예제는 해당 커밋의 컴포넌트와 ESPHome `2026.11.0`에서 `esphome config`로 검증했습니다. [기준 커밋의 UARTEx 소스](https://github.com/eigger/espcomponents/tree/e3b1d72680e30377b1ddc3c4fa3116beb0a968a0/components/uartex)

설치 예제의 `latest` 태그는 가리키는 커밋이 바뀔 수 있습니다. 예제를 같은 조건으로 재현하려면 소스 주소의 `latest`를 기준 커밋으로 바꾸세요. 다른 버전을 사용한다면 해당 버전의 공통·플랫폼별 Python 스키마에서 허용 키와 기본값을, C++ 구현에서 패킷 처리 동작을 확인하세요.

## 설치와 첫 스위치

아래는 기존 ESPHome 보드 설정에 추가하는 예제입니다. 핀과 통신 속도는 보드 배선과 장치 사양에 맞추세요. 이 예제 장치는 켜기 명령과 켜짐 상태 보고에 같은 페이로드를 사용합니다.

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [uartex]

uart:
  id: uart_bus
  tx_pin: GPIO1
  rx_pin: GPIO3
  baud_rate: 9600

uartex:
  uart_id: uart_bus
  rx_header: [0x02, 0x01]
  rx_footer: [0x0D, 0x0A]
  rx_checksum: add
  tx_header: [0x02, 0x01]
  tx_footer: [0x0D, 0x0A]
  tx_checksum: add

switch:
  - platform: uartex
    name: "거실 조명"
    state_on: [0x01, 0x01]
    state_off: [0x01, 0x00]
    command_on: [0x01, 0x01]
    command_off: [0x01, 0x00]
```

스위치를 켜면 실제 전선에는 `02 01 | 01 01 | 05 | 0D 0A`가 전송됩니다. UARTEx가 헤더, 체크섬, 푸터를 붙이므로 `command_on`에는 가운데 페이로드만 적습니다. 수신 시에는 프레임을 검사한 뒤 페이로드를 `state_on`과 비교합니다. 이 패턴은 기본적으로 앞부분이 일치하면 매칭됩니다.

## 수신 패킷을 엔티티로 읽기

같은 버스에 `02 01 | 02 03 01 | 09 | 0D 0A`라는 프레임이 들어온다고 가정합니다. 페이로드는 `02 03 01`이고, 각 바이트의 위치는 차례로 0, 1, 2입니다.

```yaml
binary_sensor:
  - platform: uartex
    name: "채널 상태"
    state: [0x02, 0x03]
    state_on: {offset: 2, data: [0x01]}
    state_off: {offset: 2, data: [0x00]}
```

`state`는 이 엔티티가 처리할 패킷을 고릅니다. `state_on`과 `state_off`도 같은 페이로드의 시작점에서 위치를 셉니다. 세 번째 바이트가 `02`라면 두 조건 모두 일치하지 않아 직전 상태를 유지합니다.

온도처럼 여러 바이트가 한 값을 표현하면 숫자 필드를 사용합니다. 아래 설정은 페이로드 `03 01 01 0A`에서 마지막 두 바이트를 큰 바이트 우선으로 읽어 `266 / 10 = 26.6`을 게시합니다.

```yaml
sensor:
  - platform: uartex
    name: "실내 온도"
    unit_of_measurement: "°C"
    state: [0x03, 0x01]
    state_number: {offset: 2, length: 2, precision: 1}
```

## 버스와 프레임 설정

| 키 | 의미와 선택 기준 |
|---|---|
| `uart_id`, `uartex_id` | 각각 UARTEx가 사용할 UART 버스와 엔티티가 사용할 UARTEx 인스턴스를 지정합니다. |
| `rx_header`, `rx_footer`, `tx_header`, `tx_footer` | 방향별 프레임 시작과 끝입니다. 바이트 배열과 ASCII 문자열을 쓸 수 있습니다. `rx_header`는 마스크와 여러 헤더 후보도 지원합니다. |
| `rx_length` | 헤더·체크섬·푸터를 포함한 전체 길이(고정값 1~256) 또는 길이를 판정하는 람다입니다. 람다는 헤더를 포함한 누적 바이트를 받아 대기 시 0, 폐기 시 음수, 길이 확정 시 양수를 반환합니다. |
| `rx_data_length` | 패킷에 길이 필드가 있을 때 사용합니다. 헤더 뒤의 `offset`, 필드의 바이트 수 `length`(기본 1), `endian`(기본 `big`), 보정값 `adjust`(기본 0)를 지정합니다. `rx_length`와는 택일합니다. |
| `rx_checksum`, `tx_checksum` | 1바이트 체크섬입니다. `add`, `xor`는 헤더와 페이로드를 계산하고 `add_no_header`, `xor_no_header`는 페이로드만 계산합니다. 사용자 람다도 지원합니다. |
| `rx_checksum2`, `tx_checksum2` | 다중 바이트 체크섬입니다. `xor_add` 또는 람다를 사용합니다. 방향별로 체크섬 설정을 하나만 선택합니다. |
| `rx_priority`, `rx_timeout` | 기본 모드 `data`는 수신 중 프레임 완성 또는 타임아웃(기본 10ms)까지 메인 루프를 점유할 수 있습니다. `loop`는 들어온 만큼 읽고 다음 루프에서 이어 읽습니다. 버스 유휴 시간만으로는 프레임 끝을 정할 수 없습니다. |
| `tx_delay`, `tx_timeout`, `tx_retry_cnt` | 마지막 송수신 이후 대기 시간(기본 50ms), ACK 대기 시간(기본 50ms), 첫 전송 이후 재시도 횟수(기본 3회)입니다. |
| `tx_ctrl_pin` | 반이중 RS-485 트랜시버의 송수신 방향 제어 핀입니다. |
| `rx_reply` | 수신 `state`에 대응하는 `command`를 응답 큐에 넣습니다. 일반 명령보다 우선하며 진행 중인 ACK 대기와 버스 대기 시간을 따릅니다. |

여러 헤더는 `rx_header: [[0xA0], [0xB0]]`처럼 후보별로 묶습니다. `[0xA0, 0xB0]`는 2바이트 헤더 하나입니다. 후보에 공통 접두사가 있으면 선언 순서와 프레임 완성 시점이 영향을 줍니다. [헤더 판정과 수신 옵션](README.md#receive-headers-rx_header)

## 상태, 명령, ACK

| 목적 | 설정 방법 |
|---|---|
| 수신 패턴 비교 | 단순 비교는 바이트 배열을 사용합니다. 특정 위치나 비트만 볼 때는 `data`, `offset`, `mask`를 지정합니다. `match: exact`는 비교 구간 끝과 페이로드 끝이 같아야 합니다. `inverted`는 비교 결과를 반전합니다. |
| 숫자 읽기 | `offset`은 필수입니다. 기본값은 `length: 1`, `signed: true`, `endian: big`, `precision: 0`입니다. `decode`는 `none`, `bcd`, `ascii`를 지원하고 바이트별 마스크는 변환 전에 적용됩니다. |
| 명령에 ACK 지정 | 예를 들어 `command_on: {data: [0x01, 0x01], ack: [0x01, 0x11]}`로 송신 바이트와 예상 응답 접두사를 지정합니다. 명령의 `mask`는 수신 ACK에 적용됩니다. |
| 다른 채널 값 유지 | 엔티티 명령의 `preserve_state`에 복사할 오프셋을 지정합니다. RX와 TX에서 같은 위치여야 하며 엔티티에 `state`가 필요합니다. 필요한 바이트를 모두 수신할 때까지 명령이 대기합니다. 전송 후에는 다음 일치 상태를 기다립니다. [상태 보존 예제](README.md#command-schema) |
| 상태 조회 | `command_update`로 조회 패킷을 보내고 `update_interval`로 주기를 정합니다(기본 60초). 피드백이 없는 스위치는 `optimistic: true`로 요청한 상태를 즉시 게시할 수 있습니다. |
| 동적 명령 | `!lambda`에서 페이로드, 필요하면 ACK와 마스크를 반환합니다. 숫자를 받는 명령은 `x`, 문자열을 받는 명령은 `str`를 사용합니다. [람다 예제](README.md#command-lambda-types) |

## 플랫폼별 설정 찾기

| 용도 | 플랫폼과 예제 |
|---|---|
| 상태 감지와 계측 | [binary_sensor](README.md#binary-sensor), [sensor](README.md#sensor), [text_sensor](README.md#text-sensor) |
| 조작과 값 입력 | [switch](README.md#switch), [button](README.md#button), [number](README.md#number), [select](README.md#select), [text](README.md#text) |
| 조명과 공조 | [light](README.md#light), [climate](README.md#climate), [fan](README.md#fan), [water_heater](README.md#water-heater) |
| 개폐와 미디어 | [cover](README.md#cover), [lock](README.md#lock), [valve](README.md#valve), [media_player](README.md#media-player) |

## 자동화와 문제 해결

자동화 액션에서 `uartex.write`로 페이로드를 전송할 수 있습니다. `data`와 선택 항목인 `ack`, `mask`를 사용합니다. `on_read`, `on_write`, `on_tx_timeout`으로 송수신과 타임아웃에 대응할 수 있습니다. [자동화 액션](README.md#actions) · [이벤트 예제](README.md#event-triggers)

| 증상 | 확인할 항목 |
|---|---|
| 아무 프레임도 잡히지 않음 | UART 핀·속도·패리티·배선, 수신 프레임의 길이 또는 종료 조건을 확인합니다. |
| 수신은 되지만 상태가 바뀌지 않음 | 헤더 뒤 첫 바이트를 0으로 세고 `state`와 개별 상태 패턴을 차례로 대조합니다. |
| ACK 타임아웃 | 실제 응답 페이로드의 시작 바이트, 마스크, 응답까지 걸린 시간과 `tx_timeout`을 대조합니다. |
| 체크섬 오류 | 헤더 포함 여부, 체크섬 바이트 수와 푸터 앞 위치를 확인합니다. |

진단용 텍스트 센서는 `uartex` 아래에 `version`, `error`, `log`를 설정해 사용합니다. `log: {disabled: false}`로 패킷 로그 센서를 켜고 `ascii: true`로 ASCII 표시를 선택할 수 있습니다. [진단 센서](README.md#diagnostic-sensors)
