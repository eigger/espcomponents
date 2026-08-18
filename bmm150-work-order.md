# BMM150 컴포넌트 정상화 작업 지시서

- 대상 리포지토리: `D:\Source\Github\espcomponents` (github://eigger/espcomponents)
- 대상 경로: `components/bmm150/`
- 작성일: 2026-08-18

---

## 0. 배경 (이 지시서만 보고 작업할 수 있도록)

`components/bmm150/`은 Bosch BMM150 3축 지자기 센서용 ESPHome external component다.
Bosch 공식 SensorAPI(`bmm150_lib.c` / `bmm150_lib.h` / `bmm150_defs.h`)를 벤더링하고,
그 위에 얇은 ESPHome 래퍼(`bmm150.cpp` / `bmm150.h` / `sensor.py`)를 씌운 구조다.

**현재 이 컴포넌트는 어떤 설정에서도 사용되고 있지 않다.** 실사용 예정 설정인
`D:\Source\CHS\colorado lvgl\new.yaml`(M5Stack Tab5 / ESP32-P4 차량용 대시보드)의
`external_components:` 목록은 `[ jaalee_jht, ble_elm327, bmi270, ws_bridge ]`이고
`platform: bmm150` 선언도 없다. 즉 **회귀 위험 없이 자유롭게 고칠 수 있다.**

**최종 목표: 차량용 나침반(방위각/heading) 제공.**
BMM150은 M5Stack Unit GNSS 모듈(BMI270 + BMM150 + BMP280) 안에 들어 있고,
같은 모듈의 BMI270 가속도계를 틸트 보정에 쓸 수 있다.

작업은 두 단계다. **Phase A를 완료·커밋한 뒤 Phase B로 넘어갈 것.**
A 없이 B를 하면 센서 부재와 버스 오류가 침묵으로 감춰져서 B를 디버깅할 수 없다.

---

## 1. 절대 지켜야 할 제약

1. **`bmm150_lib.c`, `bmm150_lib.h`, `bmm150_defs.h`는 수정하지 말 것.**
   Bosch 공식 SensorAPI 원본이며 업스트림과 재동기화 가능해야 한다.
   아래 모든 결함은 래퍼(`bmm150.cpp` / `bmm150.h` / `sensor.py`)에서만 고쳐도 전부 해결된다.
   벤더 파일을 고쳐야만 풀리는 문제가 나오면 **고치지 말고 보고할 것.**

2. **실기(하드웨어) 검증은 불가능하다.** 작업자는 컴파일 통과까지 책임진다.
   실측이 필요한 항목은 "실기 검증 필요"로 표시했다. 그 항목은 코드에 근거 주석을 남기고,
   사용자 확인 요청 목록으로 정리해 보고할 것.

3. **관련 하드웨어가 현재 불안정하다.** 이 컴포넌트가 붙을 I2C 버스(`bsp_bus`, ESP32-P4
   GPIO31/32)는 부팅 시 `Performing bus recovery`가 뜨고 주소 충돌 의심(내장 BMI270 0x68 vs
   GNSS BMI270 0x69)이 있다. **이 문제는 이 작업의 범위가 아니다.** 다만 "실기에서 안 된다"는
   보고가 오더라도 컴포넌트 탓으로 단정하지 말 것.

4. ESPHome 소스 참조 경로:
   `C:\Users\eigger\AppData\Roaming\Python\Python314\site-packages\esphome`

---

## 2. Phase A — 래퍼 레이어 결함 수정

### 요약

현재 컴포넌트는 **버스에 센서가 아예 없어도 에러 한 줄 없이 X/Y/Z를 -32768로 고정 출력한다.**
A-1 ~ A-4가 겹쳐서 생기는 현상이며, 이것을 없애는 것이 Phase A의 핵심이다.

---

### A-1. `delay_us()`가 밀리초 단위 — 1000배 (최우선)

**위치:** `components/bmm150/bmm150.cpp:89-92`

```cpp
void delay_us(uint32_t period_us, void *intf_ptr)
{
    delay(period_us);
}
```

**문제:** ESPHome의 `delay()`는 밀리초다 (`esphome/components/esp32/hal.h:35`,
`vTaskDelay(ms / portTICK_PERIOD_MS)`). Bosch 라이브러리는 마이크로초를 넘긴다.

| 호출처 | 인자 | 의도 | 실제 |
|---|---|---|---|
| `bmm150_init()` — `bmm150_lib.c:581` | `BMM150_START_UP_TIME` = 3000 | 3 ms | **3000 ms** |
| `bmm150_soft_reset()` — `bmm150_lib.c:677` | `BMM150_DELAY_SOFT_RESET` = 1000 | 1 ms | 1000 ms |
| `suspend_to_sleep_mode()` — `bmm150_lib.c:1287` | 3000 | 3 ms | 3000 ms |
| self-test — `bmm150_lib.c:1947`, `:2116` | 2000 / 4000 | 2 / 4 ms | 2 / 4 s |

**영향:** `setup()` 안에서 메인 루프가 최소 3초 블록된다. `vTaskDelay`라 WDT 패닉은 안 나지만
(a) 부팅이 3초 이상 지연되고, (b) 같은 모듈의 GPS UART RX 버퍼(ESPHome 기본 256 B,
38400 bps에서 약 70 ms면 포화)가 넘쳐 NMEA가 대량 유실되며, (c) LVGL 화면이 멈춘다.

**수정:**

```cpp
void delay_us(uint32_t period_us, void *intf_ptr)
{
    delayMicroseconds(period_us);
}
```

`delayMicroseconds()`는 `esphome/components/esp32/hal.h:43`에 있으며
`delay_microseconds_safe()`로 연결된다. 현재 `#include "esphome/core/hal.h"`만으로
`delay()`가 동작하므로 플랫폼 HAL은 빌드 시스템이 끌어온다. 컴파일 오류가 나면
`esphome/components/esp32/hal.h`를 추가할 것.

---

### A-2. `reg_read` / `reg_write` 반환값 규약이 반전됨

**위치:** `components/bmm150/bmm150.cpp:79-87`

```cpp
int8_t reg_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    return ((BMM150Component*)(intf_ptr))->read_bytes(reg_addr, reg_data, length);
}
```

**문제:** ESPHome `I2CDevice::read_bytes()` / `write_bytes()`는 `bool`을 반환한다
(`true` = 성공, `esphome/components/i2c/i2c.h:217,251`). Bosch API는 `int8_t`에
**0 = `BMM150_OK`**, 음수 = 실패를 기대한다 (`BMM150_INTF_RET_SUCCESS` = `INT8_C(0)`,
`bmm150_defs.h:408`). 성공 시 `1`을 반환하니 라이브러리 기준으로는 오류, 실패 시 `0`이라
성공으로 읽힌다. 완전 반전.

**중요:** A-2와 A-4는 **같은 커밋에서 함께 적용**할 것. A-2만 먼저 적용하면
정상 I2C 읽기도 `intf_rslt=1`이 되어 A-4의 warning이 오작동한다.

**중요:** 이 버그의 영향은 현재 A-4 때문에 **드러나지 않고 있다.**
`bmm150_get_regs()` / `bmm150_set_regs()`(`bmm150_lib.c:635`, `:607`)가 인터페이스 결과를
`dev->intf_rslt`에 저장만 하고 반환값에 반영하지 않기 때문이다. 그래도 규약은 맞춰야 한다 —
A-4에서 `intf_rslt`를 직접 읽을 것이므로 이 값이 정확해야 한다.

**수정:**

```cpp
// ESPHome의 I2C 헬퍼는 bool(true=성공)을 주고 Bosch API는 0=성공을 기대한다.
// 그대로 넘기면 성공과 실패가 뒤바뀐다.
int8_t reg_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    auto *self = (BMM150Component *) intf_ptr;
    return self->read_bytes(reg_addr, reg_data, (uint8_t) length) ? BMM150_INTF_RET_SUCCESS
                                                                 : BMM150_E_COM_FAIL;
}

int8_t reg_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    auto *self = (BMM150Component *) intf_ptr;
    return self->write_bytes(reg_addr, reg_data, (uint8_t) length) ? BMM150_INTF_RET_SUCCESS
                                                                  : BMM150_E_COM_FAIL;
}
```

`(uint8_t) length` 캐스팅도 함께 처리한다(A-7). ESPHome 시그니처의 `len`은 `uint8_t`이고
Bosch 콜백은 `uint32_t`를 넘긴다. 이 컴포넌트에서 실제 쓰이는 최대 길이는
`BMM150_LEN_XYZR_DATA` = 8 (`bmm150_defs.h:368`)과 트림 레지스터 읽기라 256 미만이므로
실질 문제는 없지만, 암묵적 축소 변환을 명시적으로 만들어 둔다.

---

### A-3. 센서가 없어도 `bmm150_init()`이 성공을 반환한다

**위치:** `components/bmm150/bmm150.cpp:69` (호출), `bmm150_lib.c:585-596` (원인)

```c
rslt = bmm150_get_regs(BMM150_REG_CHIP_ID, &chip_id, 1, dev);
if (rslt == BMM150_OK)
{
    if (chip_id == BMM150_CHIP_ID)      /* 0x32 */
    {
        dev->chip_id = chip_id;
        rslt = read_trim_registers(dev);
    }
    /* else가 없다. 칩 ID가 틀려도 rslt는 BMM150_OK 그대로 */
}
return rslt;
```

**A-2와 합쳐진 최종 실패 모드** — 버스에 BMM150이 없으면:

1. `bmm150_init()`이 `BMM150_OK`(0) 반환 → `bmm150.cpp:23`의 `ESP_LOGE("Init Error")`가 안 찍힘
2. `read_trim_registers()`가 호출되지 않아 `dev_.trim_data`가 전부 0으로 남음
3. `update()`의 `compensate_x()`에서 `data_rhall == 0 && trim_data.dig_xyz1 == 0` 경로로 빠져
   **`BMM150_OVERFLOW_OUTPUT` = -32768** 반환
4. 결과: **HA에 X/Y/Z가 -32768로 고정 표시되고 로그는 깨끗함**

**수정 방침:** 벤더 라이브러리는 칩 ID가 0x32일 때만 `dev->chip_id`를 채운다.
따라서 **라이브러리를 고치지 않고도** 호출 측에서 검증할 수 있다.

`bmm150_initialization()`:

```cpp
    rslt = bmm150_init(&dev_);
    // bmm150_init()은 칩 ID가 일치할 때만 dev_.chip_id를 채우지만, 불일치해도
    // BMM150_OK를 반환한다. 그대로 두면 센서가 없어도 트림값 0으로 초기화가
    // "성공"하고 -32768을 영원히 publish 하게 된다.
    if (rslt != BMM150_OK)
        return rslt;
    if (dev_.chip_id != BMM150_CHIP_ID)
        return BMM150_E_DEV_NOT_FOUND;   // bmm150_defs.h:133

    // chip_id 검증 통과 후에만 op_mode / presetmode 설정. 센서 부재 시 레지스터 쓰기를
    // 시도하지 않도록 bmm150_set_op_mode() 호출 전에 early return 한다.
    // read_trim_registers()는 3번 읽기를 하며 intf_rslt는 마지막 트랜잭션만 남긴다.
    // reg_read/reg_write 콜백에서 bus_error_를 래치하고 여기서 검사한다.
    if (bus_error_)
        return BMM150_E_COM_FAIL;
    // (아래 set_op_mode / set_presetmode 코드는 이 검증 뒤에 둘 것)
```

`setup()`:

```cpp
    int8_t code = bmm150_initialization();
    if (code == BMM150_OK) {
        this->initialized_ = true;
        return;
    }
    // E_DEV_NOT_FOUND는 칩 부재/오식별 — mark_failed()가 맞다.
    // E_COM_FAIL은 부팅 중 일시 NAK일 수 있다. mark_failed()는 복구 경로가 없으므로
    // warning만 걸고 update()에서 재시도한다. Component::set_retry()는 deprecated.
    if (code == BMM150_E_DEV_NOT_FOUND) {
        ESP_LOGE(TAG, "Init failed (%d)", code);
        this->mark_failed();
        return;
    }
    ESP_LOGW(TAG, "Init failed (%d), will retry", code);
    this->status_set_warning();
```

`mark_failed()`는 `esphome/core/component.h:223`에 있다.

---

### A-4. 런타임 I2C 오류가 절대 검출되지 않는다

**위치:** `components/bmm150/bmm150.cpp:35-48` (`update()`)

**문제:** `bmm150_get_regs()`(`bmm150_lib.c:635`)는 이렇게 되어 있다.

```c
dev->intf_rslt = dev->read(reg_addr, reg_data, len, dev->intf_ptr);
...
return rslt;   /* null_ptr_check 결과만 반환. intf_rslt는 반환값에 반영되지 않음 */
```

따라서 `bmm150_read_mag_data()`는 **I2C가 완전히 죽어도 `BMM150_OK`를 반환한다.**
현재 `update()`는 이 반환값만 보므로 통신 실패를 영원히 감지하지 못한다.

**수정 방침:** 라이브러리를 고치지 말고 콜백에서 `bus_error_`를 래치한다.
`intf_rslt`는 마지막 트랜잭션만 남기므로 init의 트림 3회 읽기에는 부족하다.
`update()` 진입 시 `is_failed()` 가드도 추가한다(BMI270 등 ESPHome 관례).
초기화가 아직 안 됐으면(`initialized_ == false`) 같은 `update()`에서 재시도한다.

```cpp
void BMM150Component::update()
{
    if (this->is_failed())
        return;
    if (!this->initialized_) {
        // E_DEV_NOT_FOUND → mark_failed(); E_COM_FAIL → warning 후 return
        ...
    }

    this->bus_error_ = false;
    int8_t code = bmm150_read_mag_data(&mag_data_, &dev_);
    // 반환값만 보면 I2C가 끊겨도 항상 OK로 보인다. 콜백 래치를 본다.
    if (code != BMM150_OK || this->bus_error_)
    {
        ESP_LOGW(TAG, "Read failed (rslt=%d)", code);
        this->status_set_warning();
        return;
    }
    this->status_clear_warning();

    if (this->mag_x_ != nullptr) this->mag_x_->publish_state(mag_data_.x);
    if (this->mag_y_ != nullptr) this->mag_y_->publish_state(mag_data_.y);
    if (this->mag_z_ != nullptr) this->mag_z_->publish_state(mag_data_.z);
}
```

`status_set_warning()` / `status_clear_warning()`는 `esphome/core/component.h:284,291`에 있다
(무인자 오버로드 존재).

**추가:** 값이 `BMM150_OVERFLOW_OUTPUT`(-32768)로 나오는 경우도 정상값이 아니라 보상 실패이므로,
**X/Y/Z 중 하나라도** overflow이면 publish 하지 말고 warning 처리할 것.

---

### A-5. `(int16_t)NAN` — 정의되지 않은 동작

**위치:** `components/bmm150/bmm150.cpp:17-19`

```cpp
mag_data_.x = (int16_t)NAN;
```

NaN → 정수 변환은 UB다. 또한 `int16_t`에는 "값 없음"을 표현할 방법이 없다.
`BMM150_USE_FLOATING_POINT`가 정의되어 있지 않으므로 `bmm150_mag_data`는 int16 변형이 활성이다
(`bmm150_defs.h:579` 이하 `#ifdef` 확인 완료).

**수정:** 세 줄을 삭제한다. A-4 수정으로 첫 성공 읽기 전에는 아무것도 publish 되지 않으므로
초기값 자체가 불필요해진다.

---

### A-6. `dev_` 명시적 초기화

**위치:** `components/bmm150/bmm150.h:29` (`struct bmm150_dev dev_;`)

`cg.new_Pvariable`이 생성하는 `new BMM150Component()`의 값 초기화 규칙에 의존하지 말고,
`setup()` 진입 시 명시적으로 0으로 채운다. `trim_data`가 쓰레기값이면 A-3의 검출 로직이
우회될 수 있다.

```cpp
void BMM150Component::setup()
{
    memset(&dev_, 0, sizeof(dev_));   // <cstring> 필요
    ...
}
```

---

### A-7. `length` 축소 변환 명시화

A-2 수정에 포함됨. `uint32_t length` → `(uint8_t) length`.

---

### A-8. 데드코드 제거

**위치:** `components/bmm150/bmm150.cpp:58-66`, `components/bmm150/bmm150.h:26-28`

`mag_offset_`, `mag_max_`, `mag_min_`이 초기화만 되고 **어디서도 읽히지 않는다.**
하드아이언 캘리브레이션을 넣다 만 흔적이다.

**Phase A에서는 전부 삭제한다.** Phase B에서 영구 저장(preferences)과 트리거를 포함한 제대로 된
설계로 다시 추가할 것이므로, 어중간한 잔재를 남기지 않는다.

---

### A-9. `dump_config()` 보강

**위치:** `components/bmm150/bmm150.cpp:26-31`

현재 `LOG_I2C_DEVICE`만 있다. ESPHome 관례에 맞춰 추가한다.

- `if (this->is_failed())` 시 에러 메시지
- `LOG_UPDATE_INTERVAL(this)`
- `LOG_SENSOR("  ", "Magnetic Field X", this->mag_x_)` (Y/Z도)

주변 코드(예: `components/bmi270/`)의 스타일에 맞출 것.

---

### A-10. `sensor.py` 정리

**위치:** `components/bmm150/sensor.py`

- 세 센서에 `unit_of_measurement`가 없다. **Phase A에서는 건드리지 말고 B-1 결과를 기다릴 것.**
- `CODEOWNERS = ["@eigger"]` 추가
- `accuracy_decimals=0` 명시 (정수 출력이므로)

---

### Phase A 완료 조건

- [ ] `bmm150_lib.c` / `bmm150_lib.h` / `bmm150_defs.h`가 **변경되지 않았음** (`git diff`로 확인)
- [ ] 아래 테스트 YAML로 `esphome compile` 통과
- [ ] 센서 부재 시나리오가 코드상 `mark_failed()`로 귀결됨을 리뷰로 확인
- [ ] `README.md` 예시가 유효한 스키마를 따름

테스트는 리포지토리 기존 경로 `tests/components/bmm150/`를 사용한다
(`common.yaml` + `test.esp32-idf.yaml` 등). 별도 `test/bmm150-test.yaml` 생성은 불필요.

참고용 최소 YAML:

```yaml
esphome:
  name: bmm150-test
esp32:
  board: esp32dev
  framework:
    type: esp-idf
logger:
external_components:
  - source:
      type: local
      path: components
    components: [ bmm150 ]
i2c:
  sda: GPIO21
  scl: GPIO22
sensor:
  - platform: bmm150
    address: 0x10
    update_interval: 1s
    magnetic_field_x:
      name: "MAG X"
    magnetic_field_y:
      name: "MAG Y"
    magnetic_field_z:
      name: "MAG Z"
```

---

## 3. Phase B — 방위각(heading) 구현

사용자 확정 요구사항: **차량용 나침반.** 원시 XYZ가 아니라 실제로 쓸 수 있는 방위각이 목표다.

### B-1. 출력 단위/스케일 확정 (실기 검증 필요)

`BMM150_USE_FLOATING_POINT`가 미정의라 정수 보상 경로가 활성이고,
`compensate_x/y/z()`는 `int16_t`를 반환한다. Bosch 정수 보상 출력은 **µT 단위**로 알려져 있으나
확정 전에 실측할 것.

**검증 방법:** 센서를 수평으로 놓고 한 바퀴 천천히 회전시키며 X/Y 궤적을 본다.

- 원의 반지름 ≈ 지구 자기장 수평성분. **한국(서울) 기준 약 30 µT**여야 한다
- Z는 약 -40 µT 근처 (연직 하향 성분)
- 총자기장 크기 √(x²+y²+z²) ≈ **50 µT**

이 값이 나오면 µT 확정 → `sensor.py`에 `unit_of_measurement="µT"` 추가.
ESPHome에 지자기용 `device_class`는 없으므로 생략하고 기존 icon 유지.
크게 어긋나면 스케일 계수를 찾아 코드에 상수로 두고 주석에 근거를 남길 것.

### B-2. 하드아이언 캘리브레이션

차량 대시보드는 주변 철판·스피커 자석 때문에 하드아이언 오프셋이 수십 µT 단위로 잡힌다.
보정 없이는 방위각이 크게 틀린다.

**알고리즘 (하드아이언, 최소 구현):**

- 캘리브레이션 모드 동안 축별 min/max 누적
- `offset[axis] = (max[axis] + min[axis]) / 2`
- 보정값 = `raw[axis] - offset[axis]`

**소프트아이언 (선택):** 축별 스케일 `scale[axis] = avg_delta / delta[axis]`,
`avg_delta = (dx + dy + dz) / 3`. 난이도가 낮으니 함께 넣되 옵션으로 켤 수 있게 할 것.

**주의:** 진행 중 min/max가 충분히 벌어졌는지(예: 각 축 delta > 20 µT) 검사해서 불충분하면
캘리브레이션을 채택하지 말고 경고할 것. 사용자가 8자를 대충 그렸을 때 쓰레기 오프셋이
영구 저장되는 것이 최악의 시나리오다.

### B-3. 캘리브레이션 영구 저장

매 부팅 재캘리브레이션은 비현실적이다. `ESPPreferenceObject`로 저장한다.

```cpp
struct BMM150Calibration {
    int16_t offset_x, offset_y, offset_z;
    float   scale_x,  scale_y,  scale_z;
    uint8_t valid;
} __attribute__((packed));
```

- `global_preferences->make_preference<BMM150Calibration>(this->get_object_id_hash())`
- `setup()`에서 load, 실패하거나 `valid != 1`이면 오프셋 0 / 스케일 1로 시작하고
  로그에 "캘리브레이션 필요" 경고
- 참고 구현: `esphome/components/sgp30/`의 baseline 저장 로직

### B-4. 틸트 보정 (BMI270 가속도 융합)

차량 대시보드는 기울어져 있으므로 평면 나침반 공식은 못 쓴다.

**설계 결정: BMI270 컴포넌트에 직접 의존하지 말 것.** 대신 YAML에서 가속도 센서 3개의 ID를
받는다. 그러면 BMI270이든 다른 IMU든 붙일 수 있고 컴포넌트 간 결합도 생기지 않는다.

```yaml
  - platform: bmm150
    heading:
      name: "Heading"
    accel_x_id: id_gnss_bmi270_accel_x
    accel_y_id: id_gnss_bmi270_accel_y
    accel_z_id: id_gnss_bmi270_accel_z
```

**공식 (표준 틸트 보정 나침반):**

```
roll  φ = atan2(ay, az)
pitch θ = atan2(-ax, ay·sinφ + az·cosφ)

Xh = mx·cosθ + mz·sinθ
Yh = mx·sinφ·sinθ + my·cosφ - mz·sinφ·cosθ

heading = atan2(-Yh, Xh) * 180/π
heading = fmod(heading + 360, 360)
```

**축 정렬 확인 필수 (실기 검증 필요):** M5 Unit GNSS의 BMM150과 BMI270은 기판상 축 방향이
서로 다를 수 있다. 위 공식은 두 센서의 X/Y/Z가 같은 방향이라고 가정한다.
**축 리매핑이 필요할 가능성이 높으니 하드코딩하지 말고 YAML 옵션이나 최소한 한 곳에 모인
상수로 둘 것.** 실측 전에는 "미검증" 주석을 남긴다.

가속도 센서가 설정되지 않았으면 틸트 보정 없이 평면 공식(`heading = atan2(-my, mx)`)으로
폴백하고, 그 사실을 `dump_config()`에 표시할 것.

### B-5. 자기 편각(declination) 옵션

자북과 진북은 다르다. 한국은 약 **8° 서편(-8°)**이다.
`declination:` 옵션(도 단위, 기본 0)을 받아 `true_heading = magnetic_heading + declination`,
다시 0~360 정규화. 기본값을 한국으로 하드코딩하지 말 것 — 기본 0에 문서로 안내한다.

### B-6. 캘리브레이션 트리거 설계

ESPHome 액션으로 노출한다.

```yaml
on_...:
  - bmm150.calibrate:
      id: my_bmm150
      duration: 30s
```

- 액션 실행 시 min/max 수집 시작, `duration` 경과 후 자동 종료·검증·저장
- 진행 중에는 heading을 publish 하지 않거나 `unknown` 처리
- `on_calibration_finished` 트리거 제공(성공/실패 bool 전달) → LVGL UI에서 안내 가능
- 참고: `esphome/core/automation.h`, 기존 액션 구현 예시는 `esphome/components/*/`의
  `*_action` 클래스들

### B-7. 문서화

`components/bmm150/README.md`를 전면 갱신:

- 전체 옵션 표
- 캘리브레이션 절차 (8자 그리기, 소요 시간, 성공 판정)
- 차량 설치 시 주의 (스피커·철판·전선에서 이격)
- 축 정렬·편각 설명
- 현재 예시(`0x10`, `update_interval: 30s`)는 heading 용도엔 너무 느리다.
  나침반이면 `update_interval`을 200ms~1s 수준으로 안내할 것

### Phase B 완료 조건

- [ ] `esphome compile` 통과 (heading + 캘리브레이션 포함 YAML)
- [ ] 가속도 ID 미지정 시 평면 폴백이 동작
- [ ] 캘리브레이션 미완료 상태에서 heading이 잘못된 값을 조용히 내보내지 않음
- [ ] 저장/복원 경로가 재부팅을 넘어 유지되는 구조인지 코드 리뷰로 확인
- [ ] 실기 검증 필요 항목(B-1 스케일, B-4 축 정렬)이 사용자 확인 요청 목록으로 정리됨

---

## 4. 보고 형식

작업 완료 시 다음을 보고할 것.

1. Phase A / B 각각의 변경 파일 목록과 핵심 diff 요약
2. `git diff --stat`으로 벤더 3파일 무변경 증명
3. `esphome compile` 결과
4. **실기 검증이 필요한 항목 목록** (최소: B-1 단위 스케일, B-4 축 정렬,
   실제 I2C 버스·주소 확인). B-5(편각 기본값)는 설계 확정(기본 0)이며 실기 검증 항목이 아님.
5. 구현 중 지시서와 어긋나는 판단을 했다면 그 이유

---

## 5. 참고

- Bosch BMM150 SensorAPI: https://github.com/boschsensortec/BMM150_SensorAPI
- 칩 ID `0x32` (`BMM150_REG_CHIP_ID`), I2C 주소 0x10~0x13 (CSB/SDO 스트랩), M5 Unit GNSS는 0x10
- M5Stack Unit GNSS 구성: BMI270 + BMM150(0x10) + BMP280(0x76) + GPS(UART)
- 이 지시서의 근거가 된 코드 조사는 2026-08-18 세션에서 수행됨. 인용된 줄 번호는 커밋
  `bab8c44` 시점 기준이므로, 작업 전 실제 파일에서 재확인할 것
