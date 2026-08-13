# ws_bridge (ESPHome 클라이언트) 프로토콜 동기화 작업 지시서

> **대상**: 이 컴포넌트에서 작업할 다른 AI 에이전트 또는 개발자
> **목적**: HA 통합(`hass-ws-bridge`)이 Phase 0~4에서 확장한 프로토콜에 클라이언트를 맞춘다
> **서버 기준**: `hass-ws-bridge` main (Phase 4 + 공통 계층 정리 완료, 플랫폼 27종)
> **클라이언트 현재**: 플랫폼 11종(+ tracker) — C0 `params`/`features`·C1 `update`·C2 `light`/`cover`/`fan` 반영. 남은 Tier A는 C3+.
> **작성일**: 2026-08-13
> **갱신**: 2026-08-13 — C0 머지 + C2 구현

---

## 0. 이 문서를 읽는 법

1. **§1**로 현재 격차를 파악한다.
2. **§2에서 서버측 프로토콜 문서를 먼저 읽는다.** 이 문서는 "무엇을 왜"만 다루고, 필드 스펙의 정본은 `hass-ws-bridge/docs/PROTOCOL.md`다.
3. **§3(프로토콜 계층 확장)을 반드시 먼저 끝낸다.** 이후 모든 플랫폼이 여기 의존한다.
4. **§4**에서 자기가 맡을 Phase를 고른다. 플랫폼마다 **ESPHome 도메인 유무**에 따라 구현 방식이 완전히 다르다 — §4.0을 건너뛰지 말 것.
5. 플랫폼 1종 추가 절차는 **§5 체크리스트**. 매번 그대로 따른다.
6. **§7 함정 목록은 코드를 쓰기 전에 읽는다.** 대부분 서버측에서 실제로 밟았던 것들이다.

---

## 1. 현재 격차

### 1.1 구현 완료 (8종 + tracker)

| ESPHome 플랫폼 | 디렉터리 | 방향 |
|:---|:---|:---:|
| `sensor` | `sensor/` | 읽기 |
| `binary_sensor` | `binary_sensor/` | 읽기 |
| `text_sensor` | `text_sensor/` | 읽기 |
| `switch` | `switch/` | 제어 |
| `number` | `number/` | 제어 |
| `select` | `select/` | 제어 |
| `button` | `button/` | 제어 |
| `update` | `update/` | 제어 (OTA, #296) |
| `device_tracker` | 허브 `trackers:` 인라인 (`ws_bridge_tracker.*`) | 읽기 |

구조는 이미 좋다 — §1.3의 기존 자산을 그대로 활용한다. 엔티티 래핑(`sensor_id` / `entities:`)과 `ws_bridge/sync`도 master에 있다.

### 1.2 서버가 지원하지만 클라이언트에 없는 것 (15종)

`text`, `lock`, `date`, `time`, `datetime`,
`event`, `valve`, `climate`, `humidifier`, `water_heater`, `siren`,
`alarm_control_panel`, `media_player`, `image`, `camera`

### 1.3 이미 갖춰진 자산 (새로 만들지 말 것)

| 자산 | 위치 | 용도 |
|:---|:---|:---|
| `WsBridgeDevice` 베이스 | `ws_bridge_device.h` | `ws_bridge_declare()` / `ws_bridge_handle_command()` 가상 인터페이스 |
| `add_common_entity_fields()` | `ws_bridge_entity_json.h` | `device_class` / `icon` / `entity_category` 자동 첨부 |
| `build_state_object()` | `ws_protocol.cpp` | **객체 상태 전송 — 복합 플랫폼이 전부 이걸 쓴다** |
| 재선언·재연결·ping/pong | `ws_bridge.cpp` | 손댈 일 없음 |
| 인라인 스키마 선례 | 허브 `trackers:` | ESPHome 도메인 없는 플랫폼의 참고 구현 |

### 1.4 프로토콜 계층의 실제 공백

| 공백 | 위치 | 영향 |
|:---|:---|:---|
| 상태 배치 전송 없음 | `build_state_*` 각각 1건씩 | 복합 상태 갱신 시 메시지 수 증가 (선택) |

> `WsCommand.params` / `add_features`는 C0(#300)로 구현. `ws_bridge/sync`는 #298.

---

## 2. 먼저 읽을 것 (필수)

정본 스펙은 **서버 저장소**에 있다. 이 문서는 그걸 대체하지 않는다.

- `hass-ws-bridge/docs/PROTOCOL_ko.md` — §3.1 선언 필드, §3.2 상태 값, §4 커맨드 이벤트
- 특히 **각 플랫폼의 "state (object `value`)" 절** — 클라이언트가 보낼 JSON 형태가 그대로 적혀 있다

읽을 때 다음 3가지를 반드시 확인한다. 이게 이번 확장의 핵심이고, 모르면 반드시 버그를 만든다:

1. **객체 상태는 서버에서 얕은 병합된다.** 키를 생략하면 이전 값이 유지된다. **지우려면 JSON `null`을 명시적으로 보내야 한다.**
2. **커맨드는 `value`(단일 인자) 또는 `params`(이름 있는 다중 인자) 중 하나를 쓴다.** 액션별로 정해져 있고 섞이지 않는다.
3. **`event` 플랫폼만 병합에서 제외된다** — fire-and-forget이라 매번 완결된 페이로드를 보낸다.

---

## 3. Phase C0 — 프로토콜 계층 확장 (필수 선행)

### 3.1 `WsCommand`에 `params` 추가

**문제**: 현재 `WsCommand`는 `value` 하나만 파싱한다. 서버는 light/climate/cover 등에 대해 다음을 보낸다:

```json
{"kind":"command","unique_id":"led","action":"turn_on",
 "params":{"brightness":128,"rgb_color":[255,0,0],"transition":1.5}}
```

**제약**: `parse_message()`의 주석대로 **`JsonDocument`는 파싱 종료와 함께 사라진다.** `JsonObject`를 들고 나올 수 없으므로 필요한 값을 평범한 C++ 값으로 복사해 나와야 한다.

`ws_protocol.h`:

```cpp
// 명령 params 의 한 항목. JsonDocument 수명 밖으로 값을 복사해 나온다.
// null/객체/혼합 배열은 타입 플래그를 모두 false로 남겨 param_*가 실패하게 한다.
struct WsParam {
  std::string key;
  bool is_string{false};
  std::string s;                 // is_string == true
  bool is_number{false};
  float f{0};                    // is_number == true
  bool is_bool{false};
  bool b{false};                 // is_bool == true
  bool is_array{false};
  std::vector<float> arr;        // is_array == true; 숫자 원소만
};

struct WsCommand {
  std::string unique_id;
  std::string action;
  bool has_value{false};
  float value_float{0};
  std::string value_string;
  std::vector<WsParam> params;   // 신규

  // 조회 헬퍼 — 각 플랫폼이 이걸로 읽는다.
  const WsParam *param(const char *key) const;
  bool param_float(const char *key, float &out) const;
  bool param_bool(const char *key, bool &out) const;
  bool param_string(const char *key, std::string &out) const;
  // expected_size 필수 (rgb_color=3, hs_color=2). 길이 불일치면 false.
  bool param_array(const char *key, std::vector<float> &out, size_t expected_size) const;
};
```

`parse_message()`의 `event` 분기에 추가:

```cpp
JsonObject params = event["params"].as<JsonObject>();
if (!params.isNull()) {
  for (JsonPair kv : params) {
    WsParam p;
    p.key = kv.key().c_str();
    JsonVariant v = kv.value();
    if (v.is<JsonArray>()) {
      bool all_numbers = true;
      std::vector<float> nums;
      for (JsonVariant e : v.as<JsonArray>()) {
        if (!e.is<float>()) { all_numbers = false; break; }
        nums.push_back(e.as<float>());
      }
      if (all_numbers) { p.is_array = true; p.arr = std::move(nums); }
    } else if (v.is<const char *>()) {
      p.is_string = true;
      p.s = v.as<std::string>();
    } else if (v.is<bool>()) {
      p.is_bool = true;
      p.b = v.as<bool>();
    } else if (v.is<float>()) {
      p.is_number = true;
      p.f = v.as<float>();
    }
    msg.command.params.push_back(std::move(p));
  }
}
```

> **메모리 주의**: `params`는 최대 10개 남짓이지만 `std::vector<WsParam>`은 힙을 쓴다. ESP32에서 명령은 저빈도(사용자 조작)라 문제없다. 다만 **상태 전송 경로에는 절대 같은 패턴을 쓰지 말 것** — 그쪽은 고빈도다.

### 3.2 선언에 `features` 첨부

플랫폼별로 traits를 읽어 `JsonArray`에 직접 넣는다. C0의 `add_features(initializer_list)`는
조건부 feature 목록에 맞지 않아 **삭제**했다 — cover/fan/light 모두 런타임 traits 기반이다.

### 3.3 객체 상태 전송 — 기존 것을 쓴다

`build_state_object()` / `WsBridgeComponent::send_state_object()`가 **이미 있다.** device_tracker가 쓰고 있다. 복합 플랫폼은 전부 이걸 쓴다:

```cpp
this->parent_->send_state_object(this->unique_id_, [this](JsonObject v) {
  v["state"] = this->state ? "on" : "off";
  v["brightness"] = (int) (this->remote_values.get_brightness() * 255);
});
```

**새 빌더를 만들지 말 것.**

### 3.4 (선택) 상태 배치 전송

`build_state_*`가 매번 프레임 1개에 항목 1개만 담는다. 복합 플랫폼이 늘면 프레임 수가 늘어난다. 여유가 있으면 `send_state_batch()`를 추가하되, **C0 범위 밖이다.** 먼저 동작을 맞추고 나중에 최적화한다.

### 3.5 Phase C0 완료 조건

- [x] `WsCommand::params` 파싱 + 조회 헬퍼 5종
- [x] 선언 `features`는 각 플랫폼이 traits에서 직접 첨부 (공통 `add_features` 헬퍼는 제거)
- [ ] 기존 플랫폼 **동작 변화 0** — `params`는 추가 필드일 뿐 기존 `value` 경로 불변 (리뷰/실기)
- [ ] 실제 기기 1대로 기존 엔티티 회귀 확인 (§6)

---

## 4. 플랫폼별 구현

### 4.0 먼저: 두 부류로 나뉜다 (가장 중요)

**ESPHome 2025.5.2 기준으로 확인한 결과다. 이 분류를 무시하면 만들 수 없는 것을 만들려고 시간을 버린다.**

#### Tier A — ESPHome에 대응 도메인이 있다 → 정식 플랫폼 컴포넌트

`select/` 와 **똑같은 구조**로 만든다: `<domain>/__init__.py` + `ws_bridge_<domain>.h/.cpp`, YAML은 `<domain>: - platform: ws_bridge`.

| HA 플랫폼 | ESPHome 도메인 | 베이스 클래스 |
|:---|:---|:---|
| `update` | `update` | `update::UpdateEntity` |
| `light` | `light` | `light::LightOutput` (+ `LightState`) |
| `cover` | `cover` | `cover::Cover` |
| `fan` | `fan` | `fan::Fan` |
| `text` | `text` | `text::Text` |
| `lock` | `lock` | `lock::Lock` |
| `valve` | `valve` | `valve::Valve` |
| `event` | `event` | `event::Event` |
| `climate` | `climate` | `climate::Climate` |
| `alarm_control_panel` | `alarm_control_panel` | `alarm_control_panel::AlarmControlPanel` |
| `media_player` | `media_player` | `media_player::MediaPlayer` |
| `date` / `time` / `datetime` | **`datetime`** | `datetime::DateEntity` / `TimeEntity` / `DateTimeEntity` |

> ⚠️ **`date`·`time`은 독립 도메인이 아니다.** 셋 다 `esphome/components/datetime/` 안에 있고 YAML은 `datetime: - platform: ws_bridge, type: date|time|datetime` 형태다. ESPHome의 `time:` 컴포넌트는 **시각 소스(SNTP 등)**이지 엔티티 도메인이 아니다 — 혼동 금지.

#### Tier B — ESPHome에 도메인이 없다 → 허브 인라인 스키마

`humidifier`, `water_heater`, `siren`, `camera`, `image`

`platform: ws_bridge`를 걸 도메인이 없다. **`device_tracker`가 이미 이 방식으로 되어 있으니 그대로 따른다**:
- 허브 `__init__.py`에 `TRACKER_SCHEMA` 같은 인라인 스키마 추가
- `ws_bridge_tracker.h/.cpp` 같은 전용 `WsBridgeDevice` 파생 클래스
- YAML은 `ws_bridge:` 아래 리스트로 설정

또는 README §233 "Declaring other entity types ESPHome has no domain for"의 **람다 수동 선언**으로 남긴다. **Tier B는 우선순위가 낮다 — 실사용 요구가 확인되기 전에는 착수하지 말 것.**

### 4.1 권장 Phase 분할

| Phase | 범위 | 근거 |
|:---|:---|:---|
| **C0** | 프로토콜 계층 (§3) | 전제 — `params` + `add_features` |
| **C1** | `update` | **완료** (#296). OTA 직결 |
| **C2** | `light`, `cover`, `fan` | **이번 PR**. 수요 최다. `params` 수신을 처음 실제로 씀 |
| **C3** | `text`, `lock`, `valve`, `event`, `datetime`(date/time/datetime) | 스칼라 위주, 난도 낮음 |
| **C4** | `climate` | 단독. 상태·명령 표면이 가장 넓다 |
| **C5** | `alarm_control_panel`, `media_player` | 수요 확인 후 |
| **C6** (보류) | Tier B 5종 | 요구 확인 전 착수 금지 |

---

## 5. 플랫폼 1종 추가 체크리스트 (Tier A)

**`select/`를 그대로 베껴서 시작한다.** 이미 검증된 형태다.

1. **`<domain>/__init__.py`**
   - `ws_bridge_ns.class_("WsBridge<Domain>", <domain>.<Base>, cg.Component, ws_bridge.WsBridgeDevice)`
   - `CONFIG_SCHEMA = <domain>.<domain>_schema(...).extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)`
   - `to_code()`에서 `await ws_bridge.register_ws_bridge_device(var, config)` **반드시 호출** — 빠뜨리면 허브가 이 엔티티를 모르고, 선언도 명령 라우팅도 조용히 안 된다

2. **`<domain>/ws_bridge_<domain>.h`** — `public <Base>, public Component, public WsBridgeDevice` 다중 상속

3. **`<domain>/ws_bridge_<domain>.cpp`**
   - `setup()`: 상태 변경 콜백 등록 → `parent_->send_state_*()`
   - `ws_bridge_declare()`: `send_entity_declare()` + `add_common_entity_fields()` + 플랫폼 전용 필드 + `add_features()`, **그리고 현재 상태 1회 전송** (`select.cpp`의 `if (this->has_state())` 참조)
   - `ws_bridge_handle_command()`: `command.action` 분기 → `params` 조회 → ESPHome `make_call()` 수행
   - **`control()`에서 직접 상태를 push하지 말 것** — `publish_state()`가 `setup()`의 콜백을 태우므로 이중 전송이 된다 (`select.cpp` 참조)

4. **필드 스펙은 `PROTOCOL_ko.md`에서 그대로 가져온다.** 이 문서에 재기술하지 않는다 — 두 벌이 되면 반드시 어긋난다.

5. **README.md 갱신**
   - §121 "Platform options" 목록에 신규 플랫폼 추가
   - 플랫폼별 YAML 예제 1개
   - Tier B라면 §233 수동 선언 절에 추가

---

## 6. 검증

이 저장소에는 **파이썬 단위 테스트 인프라가 없다** (서버측과 다르다). 검증은 실제 컴파일과 기기다.

```bash
esphome compile <your-test-device>.yaml
```

각 Phase마다 최소한 다음을 확인한다:

1. **컴파일 통과** (ESP-IDF, `esp32:` `framework: type: esp-idf` 필수 — 허브가 강제한다)
2. **선언 왕복**: 기기 부팅 → HA에 엔티티 생성 확인
3. **상태 반영**: 기기에서 값 변경 → HA UI 반영
4. **명령 왕복**: HA UI 조작 → 기기 로그에 `params` 포함 수신 확인 → 상태 되돌아옴
5. **재연결**: HA 재시작 → 엔티티가 재선언되고 상태 복구
6. **기존 7종 회귀**: 매 Phase마다 sensor/switch 하나씩은 반드시 같이 확인

---

## 7. 함정 목록

**대부분 서버측 구현에서 실제로 밟았던 것들이다.**

1. **`register_ws_bridge_device()` 누락** → 엔티티가 선언되지도, 명령을 받지도 않는다. 에러도 로그도 없다. **가장 흔한 실수.**

2. **부분 객체 상태로 키를 지울 수 없다.** 서버가 얕은 병합을 한다. `{"brightness": 200}`만 보내면 나머지는 유지된다. **지우려면 `null`을 명시**해야 한다:
   ```cpp
   v["release_url"] = nullptr;   // 지움
   // v 에서 키를 빼는 것 = "변경 없음", 지움이 아니다
   ```

3. **`update` 플랫폼의 `in_progress`는 반드시 명시적으로 내려야 한다.** 서버는 사용자가 Install을 누르면 `in_progress: true`를 **메모리에만** 두지만, 클라이언트가 완료 후 `{"in_progress": false}`를 안 보내면 HA는 계속 설치 중으로 본다. 플래시 성공 후 리부팅하면 재선언 시점에 반드시 포함할 것.

4. **`event`는 매번 완결된 페이로드로 보낸다.** 서버가 병합하지 않으므로 `attributes`를 생략하면 그 이벤트에는 속성이 없는 게 맞다. 반대로 이전 이벤트의 속성이 새는 일도 없다.

5. **색상·좌표는 JSON 배열로 보낸다.** `rgb_color: [255,0,0]`. 서버가 `list`를 기대한다.

6. **`value`와 `params`를 한 액션에서 섞지 말 것.** 서버가 액션별로 하나만 쓴다. 파싱은 둘 다 하되, 액션별로 어느 쪽을 읽을지는 `PROTOCOL_ko.md` §4 표를 따른다.

7. **`JsonDocument` 수명**: `parse_message()`가 반환되면 파싱 문서는 사라진다. `JsonObject`/`JsonArray`를 구조체에 담아 나오면 **dangling**이다. 반드시 값 복사. (`ws_protocol.h` 주석에 이미 경고돼 있다.)

8. **`date`/`time`은 `datetime` 컴포넌트 소속이다** (§4.0). `time/` 디렉터리를 만들면 ESPHome의 시각 소스 컴포넌트와 충돌한다.

9. **`text`와 `text_sensor`는 완전히 다르다.** `text_sensor`(읽기 전용)는 이미 있고, `text`(쓰기 가능)는 신규다. 서버측 `min`/`max`도 의미가 다르다 — `number`는 값 범위, `text`는 **문자열 길이**.

10. **상태 전송 경로에서 힙 할당을 늘리지 말 것.** 고빈도 경로다. `params` 파싱(저빈도)과 다르게 취급한다.

11. **ESP-IDF 전용**: 허브가 `_validate_esp_idf()`로 강제한다. Arduino 프레임워크 예제를 만들지 말 것.

12. **선언 시 현재 상태를 함께 보낸다.** `ws_bridge_declare()` 끝에서 상태 1회 전송을 빼먹으면, 재연결 직후 HA가 엔티티는 만들지만 값이 비어 있다. `select.cpp` 참조.

---

## 8. Phase 완료 조건 (DoD)

- [ ] `esphome compile` 통과 (ESP-IDF)
- [ ] §5 체크리스트 5항목 수행
- [ ] §6 검증 6항목 중 최소 1~4 + 6 실기 확인
- [ ] README.md 갱신 (플랫폼 목록 + YAML 예제)
- [ ] 기존 7종 플랫폼 회귀 없음
- [ ] `PROTOCOL_ko.md`와 어긋난 부분이 없는지 대조 — **어긋나면 서버가 정본이다**

---

## 9. 서버측에 확인이 필요할 수 있는 항목

구현 중 아래를 만나면 임의 판단하지 말고 서버 저장소 이슈로 제기한다.

- **`camera` / `image`**: 서버는 **URL 전용**이다 (base64/바이너리 전송 금지). ESP32가 자체 HTTP 서버로 스틸 이미지를 제공할 수 있는지가 선결 문제다. 없으면 Tier B에서도 제외.
- **`media_player`**: 서버측이 1차 범위(재생/볼륨/소스)만 구현했다. `browse_media`/`play_media`는 양쪽 다 미구현.
- **`device_class` 검증**: 서버가 대부분의 플랫폼에서 문자열을 그대로 통과시킨다. 오타를 보내면 HA가 애매한 등록 오류를 낸다 — 클라이언트에서 유효값을 확인하고 보낼 것.
