# UARTEx 한국어 사용 안내

UARTEx는 UART 또는 RS-485로 주고받는 장치의 패킷을 ESPHome 엔티티에 연결하는 컴포넌트입니다. 장치의 전송 속도와 프레임 형식이 필요합니다. 월패드를 연결한다면 제조사별 프리셋에서 같은 모델의 설정을 먼저 확인하세요.

[English README](README.md) · [AI 에이전트용 패킷 구성 지침(영문)](PACKET_GUIDE.md) · [월패드 프리셋](../../packages/wallpad) · [TCP 포워딩](../tcp_server)

## 문서 기준

2026-11-21에 확인한 espcomponents 커밋 `e3b1d72`의 UARTEx를 기준으로 작성했습니다. 아래 16개 플랫폼의 YAML 예제를 로컬 컴포넌트와 ESPHome `2026.11.0`으로 설정 검사하고 ESP32용으로 빌드했습니다. 제조사 프리셋에서 가져온 사례와 설명용 패킷은 각 절에 구분해 표시했습니다. [기준 커밋의 UARTEx 소스](https://github.com/eigger/espcomponents/tree/e3b1d72680e30377b1ddc3c4fa3116beb0a968a0/components/uartex)

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

길이 필드가 있는 프레임 `AA BB | 03 11 22 33 | CE`를 예로 들면, 헤더 길이는 2, 길이 필드 위치는 페이로드 오프셋 0, 필드 폭은 1입니다. `03`은 뒤따르는 데이터 3바이트를 세므로 `rx_data_length: {offset: 0, length: 1, adjust: 0}`과 `rx_checksum: add`로 전체 7바이트를 해석합니다. 여기서 길이 필드 자체도 수신 페이로드에 남습니다.

## 상태, 명령, ACK

| 목적 | 설정 방법 |
|---|---|
| 수신 패턴 비교 | 단순 비교는 바이트 배열을 사용합니다. 특정 위치나 비트만 볼 때는 `data`, `offset`, `mask`를 지정합니다. `match: exact`는 비교 구간 끝과 페이로드 끝이 같아야 합니다. `inverted`는 비교 결과를 반전합니다. |
| 숫자 읽기 | `offset`은 필수입니다. 기본값은 `length: 1`, `signed: true`, `endian: big`, `precision: 0`입니다. `decode`는 `none`, `bcd`, `ascii`를 지원하고 바이트별 마스크는 변환 전에 적용됩니다. |
| 명령에 ACK 지정 | 예를 들어 `command_on: {data: [0x01, 0x01], ack: [0x01, 0x11]}`로 송신 바이트와 예상 응답 접두사를 지정합니다. 명령의 `mask`는 수신 ACK에 적용됩니다. |
| 다른 채널 값 유지 | 엔티티 명령의 `preserve_state`에 복사할 오프셋을 지정합니다. RX와 TX에서 같은 위치여야 하며 엔티티에 `state`가 필요합니다. 필요한 바이트를 모두 수신할 때까지 명령이 대기합니다. 전송 후에는 다음 일치 상태를 기다립니다. [상태 보존 예제](README.md#command-schema) |
| 수신 직후 송신 허용 | 장치가 정해진 프레임 뒤에만 송신할 수 있으면 `state_response`로 그 프레임을 지정합니다. 가장 최근 수신값이 일치해야 엔티티 큐에서 명령 하나를 꺼낼 수 있습니다. 이미 보낸 명령의 확인 응답은 해당 명령의 `ack`에 지정합니다. |
| 상태 조회 | `command_update`로 조회 패킷을 보내고 `update_interval`로 주기를 정합니다(기본 60초). `optimistic`(기본 `true`)이 켜져 있으면 HA에서 내린 명령의 요청 상태를 즉시 반영하며, `false`로 지정하면 명령은 보내되 상태 피드백이 올 때까지 상태를 바꾸지 않습니다. 이전에는 스위치가 기본적으로 피드백을 기다렸으므로 그 동작이 필요하면 `optimistic: false`를 지정하세요. 명령이 설정되지 않은 엔티티는 예전에는 요청을 무시했지만, 기본값에서는 아무것도 보내지 않고 요청을 반영하므로 이전 동작이 필요하면 `optimistic: false`를 지정하세요. 명령 람다가 데이터를 반환하지 않으면 명시적인 거부로 취급되어, `optimistic: true`여도 아무것도 보내지 않고 요청 상태도 반영하지 않습니다. `optimistic`은 제어 가능한 플랫폼(`switch`, `climate`, `cover`, `fan`, `lock`, `media_player`, `number`, `select`, `text`, `valve`, `water_heater`)에서만 사용할 수 있습니다. `light`는 ESPHome 조명 상태가 요청을 즉시 반영하므로 이 옵션을 지원하지 않고, `sensor`, `binary_sensor`, `text_sensor`, `button`도 받지 않습니다. 해당 플랫폼에 `optimistic`을 지정한 설정은 이제 검증에 실패하므로 키를 제거하세요. |
| 상태 깜빡임 필터 | 월패드가 `off`↔`heat`처럼 1~2초짜리 일시 상태를 보낼 때 `state_debounce`(기본 `0`, 비활성, 최대 `60s`)를 지정합니다. 엔티티에 `state`가 필요합니다. 기본은 일치한 패킷 전체를 비교하므로 같은 패킷 안의 다른 바이트(예: 현재 온도)가 계속 바뀌면 그 패킷도 지연되고, 패킷 안의 모든 값이 최대 지정 시간만큼 늦게 반영됩니다. 이를 피하려면 아래의 `offset`/`mask` 형식으로 일부 바이트만 비교합니다. 일치한 상태 패킷을 보류했다가 변화가 시작된 시점부터 지정 시간이 지나면 게시하며, 그동안 마지막으로 받은 패킷이 적용됩니다. 마지막으로 적용된 상태와 같은(`mask`를 쓰면 마스크 영역이 같은) 패킷이 오면 보류를 취소하고 즉시 다시 게시합니다. 부팅 후 첫 패킷과, 엔티티 자신의 명령 전송 직후(지정 시간 이내) 처음 수신한 상태 변화는 즉시 적용하므로 `optimistic: false`여도 명령 결과가 지연되지 않습니다(`command_update` 폴링은 해당하지 않고, 마지막 적용 상태와 같은 패킷은 이를 소모하지 않음). 엔티티가 마지막으로 전송한 명령 이후의 첫 상태 변화만 즉시 적용되므로, `ack` 없이 같은 엔티티의 명령이 연달아 나가면 뒤의 결과는 일반 디바운스를 따릅니다. 이 우회는 엔티티의 일반 명령 큐(제어 동작)로 나간 명령이 켜므로, 사용자 람다가 `enqueue_tx_cmd`를 주기적으로 호출하면 계속 켜져 있게 됩니다. `state_response`는 지연하지 않습니다. `optimistic: true`(기본)이면 HA에서 내린 명령은 `control`로 즉시 반영되고, `optimistic: false`이면 상태 피드백이 올 때만 반영됩니다. 버스에서 관찰한 변화는 최대 지정 시간만큼 늦어집니다. 변화가 보류 중이면 `preserve_state` 명령도 안정될 때까지(보통 지정 시간 이내, 패킷의 일부 바이트가 계속 바뀌면 더 길어질 수 있음) 대기합니다. |
| 동적 명령 | `!lambda`에서 페이로드, 필요하면 ACK와 마스크를 반환합니다. 숫자를 받는 명령은 `x`, 문자열을 받는 명령은 `str`를 사용합니다. [람다 예제](README.md#command-lambda-types) |

```yaml
climate:
  - platform: uartex
    name: "거실 난방"
    state:
      data: [0x02, 0x03]
    state_debounce: 2s
```

먼저 시간만 지정해 보고, 같은 패킷의 다른 값(예: 현재 온도)이 지연되면 `mask`를 추가해 글리치 판단에 필요한 바이트만 비교하세요.

```yaml
    state_debounce:
      delay: 2s
      offset: 3              # 선택, 기본 0
      mask: [0xFF, 0x0F]     # 이 비트/바이트만 비교
```

`offset`과 `mask`의 의미는 `state`와 같습니다(`offset`부터 마스크 적용). 이 형식에서 `mask`는 필수(비어 있으면 안 됨)이며 `delay`는 최대 `60s`입니다. `delay: 0s`와 `mask`를 함께 쓰면 마스크는 무시됩니다(디바운스 비활성). 마스크를 쓰면 마스크 영역이 마지막으로 적용된 패킷과 같을 때(대기 중인 변화 취소 포함) 패킷을 즉시 적용하므로 현재 온도 같은 비교 제외 바이트는 지연 없이 갱신됩니다. 마스크 영역이 바뀌면 `delay` 동안 보류한 뒤 마지막으로 받은 패킷 전체를 적용합니다. 마스크 범위의 바이트가 한쪽 패킷에만 있으면 다른 값으로, 양쪽 모두 없으면 같은 값으로 취급합니다.

## 플랫폼별 설정 예제

앞의 스위치(`switch`), 상태 감지기(`binary_sensor`), 온도 센서(`sensor`)에 이어 나머지 플랫폼을 설명합니다. 아래 예제는 **각각 독립적인 예시 프로토콜**입니다. 같은 버스에서 여러 엔티티를 사용할 때는 각 장치의 주소와 보고 종류를 `state`로 구분하세요. 숫자 센서는 `state_number` 또는 `lambda` 중 하나를 설정합니다.

### 버튼 (`button`)

엘리베이터 호출처럼 한 번만 보내는 동작입니다. 누를 때 `command_on`을 전송합니다. 버튼에는 상태 필터나 주기 조회를 설정할 수 없습니다.

```yaml
button:
  - platform: uartex
    name: "Elevator Call"
    command_on: [0x02, 0x03, 0x01]
```

### 밝기 조절 조명 (`light`)

이 예제의 RX 페이로드 `01 01 80`은 켜짐과 밝기 약 50%를 뜻합니다. UARTEx 조명의 밝기 단위는 수신과 송신 모두 퍼센트입니다. 장치가 0~255를 쓰므로 양방향으로 환산합니다. 전원만 제어하는 조명은 밝기 필드를 생략합니다.

```yaml
light:
  - platform: uartex
    name: "Room Light"
    state_on: [0x01, 0x01]
    state_off: [0x01, 0x00]
    state_brightness: !lambda |-
      return len > 2 ? data[2] * 100.0f / 255.0f : NAN;
    command_on: [0x01, 0x01]
    command_off: [0x01, 0x00]
    command_brightness: !lambda |-
      return {{0x01, 0x02, (uint8_t) lroundf(x * 255.0f / 100.0f)}, {0x01, 0x12}};
```

### 상태 문구 읽기 (`text_sensor`)

`02 03 01`이면 Running, `02 03 02`이면 Idle을 게시합니다. `text_sensor`에는 문자열을 반환하는 `lambda`가 필요합니다. 첫 두 바이트만 도착해도 접두사는 일치하므로 세 번째 바이트를 읽기 전에 길이를 확인합니다.

```yaml
text_sensor:
  - platform: uartex
    name: "Status"
    state: [0x02, 0x03]
    lambda: |-
      if (len < 3) return "Unknown";
      if (data[2] == 0x01) return "Running";
      if (data[2] == 0x02) return "Idle";
      return "Unknown";
```

### 수치 입력 (`number`)

사용자가 1~10 중 5를 선택하면 `command_number`의 `x`가 5이고 TX 페이로드는 `02 01 05`입니다. 수신값은 오프셋 2에서 읽습니다. 장치가 절대값 대신 증감 이벤트를 보내면 `state_increment`, `state_decrement`도 사용할 수 있습니다.

```yaml
number:
  - platform: uartex
    name: "Speed Level"
    min_value: 1
    max_value: 10
    step: 1
    state_number:
      offset: 2
    command_number: !lambda |-
      return {{0x02, 0x01, (uint8_t)x}, {0x02, 0x11}};
```

### 선택 목록 (`select`)

장치의 열거값과 화면의 선택지를 양방향으로 대응시킵니다. `options`는 필수이며 `state_select`의 반환 문자열이 목록에 있을 때만 수신값을 게시합니다. 명령 람다의 `str`에는 선택한 문자열이 들어옵니다.

```yaml
select:
  - platform: uartex
    name: "Mode"
    options:
      - "Auto"
      - "Manual"
      - "Off"
    state_select: !lambda |-
      if (len < 3) return "";
      if (data[2] == 0x01) return "Auto";
      if (data[2] == 0x02) return "Manual";
      return data[2] == 0 ? "Off" : "";
    command_select: !lambda |-
      if (str == "Auto") return {{0x02, 0x01}, {}};
      if (str == "Manual") return {{0x02, 0x02}, {}};
      return {{0x02, 0x00}, {}};
```

### 문자열 입력 (`text`)

표시기에 보낼 문자열 앞에 명령 코드 10을 붙이는 예제입니다. ABC를 입력하면 TX 페이로드는 `10 41 42 43`입니다. `command_text`는 필수이며, 입력값은 `str`로 전달됩니다.

```yaml
text:
  - platform: uartex
    name: "Message"
    mode: text
    command_text: !lambda |-
      std::vector<uint8_t> cmd = {0x10};
      for (char c : str) cmd.push_back(c);
      return {cmd, {}};
```

### 냉난방기 (`climate`)

RX 페이로드 `01 18 16`은 난방, 목표 24°C, 현재 22°C입니다. 모드와 목표 온도를 한 패킷으로 보내는 장치는 모드 명령에서 `use_command_target_temperature()`로 이번 요청의 목표값을 읽습니다. 모드 명령이 이번 호출에서 목표값을 읽으면 그 요청에 대해 별도의 온도 명령은 절대 보내지 않으며, 패킷이 큐에 들어가고 `optimistic`이 true이면 목표값도 즉시 반영합니다. 람다가 데이터를 반환하지 않으면 모드와 목표값을 모두 거부합니다. `optimistic: false`이면 `command_temperature`나 `command_preset`에서 `id(thermostat).mode`처럼 엔티티 자신의 현재 상태를 읽는 람다는 같은 호출에서 요청한 값이 아니라 마지막으로 확인된 상태를 봅니다. 모드와 온도 명령이 독립적인 장치는 각 명령을 따로 정의합니다. 현재 동작(가열·대기)은 `state_action_heating`·`state_action_idle`, 풍향은 `state_swing_*`, 습도는 `state_humidity_current`·`state_humidity_target`으로 표현합니다.

```yaml
climate:
  - platform: uartex
    name: "Thermostat"
    id: thermostat
    visual:
      min_temperature: 16
      max_temperature: 30
      temperature_step: 1
    state_off: [0x00]
    state_heat: [0x01]
    state_temperature_target:
      offset: 1
    state_temperature_current:
      offset: 2
    command_off: [0x03, 0x00]
    command_heat: !lambda |-
      float target = id(thermostat).use_command_target_temperature();
      if (!(target >= 16 && target <= 30)) return {};
      return {{0x03, 0x01, static_cast<uint8_t>(target)}};
    command_temperature: !lambda |-
      return {{0x03, 0x01, (uint8_t)x}, {}};
```

실제 CVNet 난방 1 프리셋에서는 오프셋 4 한 바이트에 난방 여부와 목표 온도가 함께 들어갑니다. 예를 들어 `0x98`의 상위 비트가 켜져 있으면 난방이고, `0x98 & 0x7F`가 24이므로 목표는 24°C입니다. 오프셋 5가 `0x16`이면 현재 온도는 22°C입니다. 같은 필드에서 모드에는 `mask: [0x80]`, 목표값에는 `mask: [0x7F]`를 적용할 수 있습니다. [CVNet 난방 프리셋](../../packages/wallpad/cvnet/cvnet.yaml)

### 환기팬 (`fan`)

RX 페이로드 `01 03`은 켜짐과 3단입니다. `state_speed`와 `command_speed`는 필수이며 `x`는 선택한 단계입니다. 단계 수는 `speed_cnt`(기본 3)로 정합니다. 풍량 바이트가 1·2·3과 다르면 실제 코드표에 맞춰 양방향으로 변환합니다.

```yaml
fan:
  - platform: uartex
    name: "Ventilator"
    state_on: [0x01]
    state_off: [0x00]
    state_speed:
      offset: 1
    command_on: [0x04, 0x01]
    command_off: [0x04, 0x00]
    command_speed: !lambda |-
      return {{0x04, 0x01, (uint8_t)x}, {}};
```

CVNet 환기팬은 전원이 오프셋 4, 풍량이 오프셋 6에 있습니다. 2단 요청은 `20 71 01 11 01 01 02 00 00 00 00 00`을 보내며, 확인 응답은 `20 01 71 91`로 시작합니다. 이 접두사와 상태 보고의 접두사 `20 01 71`가 겹치므로 새 모델에 적용할 때는 ACK도 상태 필터에 대입해 보세요. [CVNet 환기팬 프리셋](../../packages/wallpad/cvnet/cvnet.yaml)

### 블라인드 (`cover`)

이 예제에서 첫 바이트는 끝 위치 표시(01 완전 열림, 00 완전 닫힘, 02 중간), 둘째는 개방률입니다. `02 32`는 50%이므로 `precision: 2`로 0.50을 게시합니다. 목표 위치 0과 1은 각각 닫기·열기 명령, 중간값은 `command_position`으로 전달됩니다.

```yaml
cover:
  - platform: uartex
    name: "Blinds"
    state_open: [0x01]
    state_closed: [0x00]
    state_position:
      offset: 1
      precision: 2
    command_open: [0x05, 0x01]
    command_close: [0x05, 0x00]
    command_stop: [0x05, 0x02]
    command_position: !lambda |-
      return {{0x05, 0x03, (uint8_t)(x * 100)}, {}};
```

### 잠금장치 (`lock`)

잠김·풀림을 각각 보고하는 장치입니다. 동작 중이나 걸림을 별도로 보고하면 `state_locking`, `state_unlocking`, `state_jammed`를 추가합니다. TX의 잠금 명령과 RX의 잠김 완료를 장치 설명서에서 구분하세요.

```yaml
lock:
  - platform: uartex
    name: "Door Lock"
    state_locked: [0x01]
    state_unlocked: [0x00]
    command_lock: [0x06, 0x01]
    command_unlock: [0x06, 0x00]
```

### 밸브 (`valve`)

예제 장치는 열기와 닫기를 모두 지원합니다. 닫기만 가능한 가스 밸브라면 `state_open`·`state_closed`와 확인된 `command_close`만 설정합니다. 중간 위치(0.0과 1.0 사이)는 `command_position: !lambda` (`x`는 0.0~1.0)로 전달하며, 0.0과 1.0은 `command_close`·`command_open`을 사용하며, 해당 명령이 없으면 `command_position`으로 전달합니다. `command_position` 또는 `state_position`이 있으면 위치 제어가 활성화됩니다.

```yaml
valve:
  - platform: uartex
    name: "Water Valve"
    state_open: [0x01]
    state_closed: [0x00]
    command_open: [0x07, 0x01]
    command_close: [0x07, 0x00]
```

### 온수기 (`water_heater`)

RX 페이로드 `06 28 32`는 가스 모드, 목표 40°C, 현재 50°C입니다. 운전 모드와 목표 온도는 별도 명령으로 설정하며, 외출을 지원하면 `state_away_on/off`와 `command_away_on/off`에 해당하는 개별 키를 사용합니다. `optimistic: true`이면 설정되지 않은 외출 명령은 전송 없이 상태만 반영됩니다. 켜기/끄기 제어는 `command_on` 또는 `state_on`이 있을 때만 제공되며, 끄기는 `off` 모드(`command_off`/`state_off`)로 항상 가능합니다.

```yaml
water_heater:
  - platform: uartex
    name: "Water Heater"
    visual:
      min_temperature: 30
      max_temperature: 75
      target_temperature_step: 1
    state_off: [0x00]
    state_eco: [0x01]
    state_electric: [0x02]
    state_performance: [0x03]
    state_high_demand: [0x04]
    state_heat_pump: [0x05]
    state_gas: [0x06]
    state_temperature_target:
      offset: 1
    state_temperature_current:
      offset: 2
    command_off: [0x09, 0x00]
    command_eco: [0x09, 0x01]
    command_electric: [0x09, 0x02]
    command_gas: [0x09, 0x06]
    command_temperature: !lambda |-
      return {{0x09, 0x10, (uint8_t)x}, {}};
```

### 오디오 재생기 (`media_player`)

RX 페이로드 `01 32`는 재생 중이며 음량은 50%입니다. `state_volume`은 0~1이므로 50을 0.50으로 변환합니다. 반대로 `command_volume`의 `x`는 이미 0~100이므로 50%를 요청하면 TX는 `08 10 32`입니다.

```yaml
media_player:
  - platform: uartex
    name: "Audio Player"
    state_playing: [0x01]
    state_paused: [0x02]
    state_idle: [0x00]
    state_volume:
      offset: 1
      precision: 2
    command_play: [0x08, 0x01]
    command_pause: [0x08, 0x02]
    command_volume: !lambda |-
      return {{0x08, 0x10, (uint8_t) lroundf(x)}, {}};
```

## 자동화와 문제 해결

자동화 액션에서 `uartex.write`로 페이로드를 전송할 수 있습니다. `data`와 선택 항목인 `ack`, `mask`를 사용합니다. `on_read`, `on_write`, `on_tx_timeout`으로 송수신과 타임아웃에 대응할 수 있습니다. [자동화 액션](README.md#actions) · [이벤트 예제](README.md#event-triggers)

예를 들어 온도 조회 페이로드가 `03 01`인 장치는 자동화 액션에 다음처럼 쓸 수 있습니다. 주기적으로 해당 센서만 조회하려면 센서에 `command_update: [0x03, 0x01]`과 `update_interval`을 지정하세요.

```yaml
script:
  - id: request_temperature
    then:
      - uartex.write:
          data: [0x03, 0x01]
```

로그를 원본 캡처와 대조할 때는 위치의 기준에 주의하세요. `on_read`·`on_write`의 `data`는 헤더를 포함하는 전체 프레임이고 엔티티의 `state_number`·`lambda`는 페이로드를 받습니다. 수신에 자동 응답할 때는 버스의 `rx_reply`에 수신 패턴과 보낼 페이로드를 한 쌍씩 지정합니다. 예를 들어 `rx_reply: [{state: [0x51], command: [0xA0, 0x51]}]`는 51로 시작하는 수신 페이로드에 A0 51로 응답합니다.

| 증상 | 확인할 항목 |
|---|---|
| 아무 프레임도 잡히지 않음 | UART 핀·속도·패리티·배선, 수신 프레임의 길이 또는 종료 조건을 확인합니다. |
| 수신은 되지만 상태가 바뀌지 않음 | 헤더 뒤 첫 바이트를 0으로 세고 `state`와 개별 상태 패턴을 차례로 대조합니다. |
| ACK 타임아웃 | 실제 응답 페이로드의 시작 바이트, 마스크, 응답까지 걸린 시간과 `tx_timeout`을 대조합니다. |
| 체크섬 오류 | 헤더 포함 여부, 체크섬 바이트 수와 푸터 앞 위치를 확인합니다. |

진단용 텍스트 센서는 `uartex` 아래에 `version`, `error`, `log`를 설정해 사용합니다. `log: {disabled: false}`로 패킷 로그 센서를 켜고 `ascii: true`로 ASCII 표시를 선택할 수 있습니다. [진단 센서](README.md#diagnostic-sensors)
