# lvgl_fx 설계 문서

ESPHome LVGL 대시보드용 범용 모션 컴포넌트.

- 기준: ESPHome **2026.11.0** / LVGL **9.6.0**
- 진행 방식: **컴포넌트 구조를 먼저 설계한다.** Colorado Tab5 대시보드는 첫 적용 대상이자 검증용이다.
  적용 중 생기는 예외는 [10절 규칙](#10-예외-흡수-규칙)에 따라 흡수한다.
- 상태: 설계 검토 중. 코드 없음.

표시: **[확정]** 합의됨 (2026-11-19) · **[보류]** 적용 중 결정

---

## 1. 설계 원칙

1. **구성은 fx에서, 실행·중단은 호출에서.**
   효과가 어떤 속성을 얼마나, 몇 ms, 어떤 순서로 움직이는지는 `lvgl_fx:` 설정에 선언한다.
   호출하는 쪽은 "이 위젯을 이 값으로 바꿔라", "이 효과를 재생/중단해라"만 말하고,
   그때그때 달라지는 **값**(게이지 값, 텍스트, 색)만 넘긴다.
2. **정의 없이도 바로 쓸 수 있다.** 자주 쓰는 움직임은 기본 제공된다. 정의는 바꾸고 싶을 때만 한다.
3. **화면 상태는 항상 맞아야 한다.** 움직임은 생략해도 되지만 상태는 생략하면 안 된다.
   억제·중단·충돌이 생겨도 최종 값(보임/숨김, 값, 색)은 정확해야 한다.
4. **익숙한 모델을 따른다.** CSS의 *transition*(속성이 바뀔 때의 움직임)과 *animation*(재생하는 움직임)을
   그대로 빌린다. LVGL `lv_anim`을 그대로 쓰고 자체 스케줄러는 만들지 않는다.
5. **안 쓰면 비용 0.** 위젯 종류별 코드는 `LV_USE_*` 조건부 컴파일. 프레임마다 heap 할당 없음.

---

## 2. 사용 모델 한눈에

움직임은 두 종류다. 이 구분이 API, 정의 방식, 충돌·억제 규칙을 모두 결정한다.

| | **Transition** (상태 전환) | **Effect** (재생 효과) |
|---|---|---|
| 의미 | 속성이 새 값으로 가서 머문다 | 잠깐 움직였다가 원래로 돌아온다 |
| 예 | 보임/숨김, 투명도, 게이지 값, 텍스트, 색, 위치 | pulse, shake, nudge, flash, glow |
| 호출 | `lvgl_fx.set` / `fx::set_*()` — **값을 넘긴다** | `lvgl_fx.play` / `fx::play()` — **이름만** |
| 정의 | `transitions:` | `effects:` |
| 모양 | `style`: fade, slide, zoom, roll, count … | `tracks`: 재료 조합 |
| 반복 | 없음 | `repeat`: 1회, N회, 무한 (`stop`까지) |
| 끝난 뒤 | 새 값 | 원래 값 |

```yaml
# 정의 없이 바로 사용 (기본 제공 transition, effect)
- lvgl_fx.set:  { id: popup, visible: true }
- lvgl_fx.set:  { id: bar_fuel, value: !lambda return x; }
- lvgl_fx.set:  { id: lbl_speed, text: !lambda return str_sprintf("%.0f", x); }
- lvgl_fx.play: { id: lbl_prnd, effect: pulse }
- lvgl_fx.stop: { id: lbl_prnd }
```

```cpp
// lambda에서
fx::set_visible(id(popup), true);
fx::set_value(id(bar_fuel), x);
fx::set_text(id(lbl_speed), "42");
fx::play(id(lbl_prnd), fx::PULSE);
```

---

## 3. 호출 API — [확정]

호출 쪽이 넘길 수 있는 것은 **대상, 값, (선택) 이름 붙은 transition/effect, (선택) force** 뿐이다.
시간, 거리, 곡선 같은 구성 값은 넘길 수 없다 (원칙 1).

### 3.1 YAML action (4개)

| action | 필드 |
|---|---|
| `lvgl_fx.set` | `id`, 속성 하나(`visible`/`opacity`/`value`/`text`/`color`/`x`/`y`/`rotation`), `transition`(선택), `force`(선택) |
| `lvgl_fx.play` | `id`, `effect`, `force`(선택) |
| `lvgl_fx.stop` | `id`, `effect`(선택, 없으면 대상의 모든 움직임) |
| `lvgl_fx.start` | 없음. 준비 완료(ready) 선언 |

- 모든 값은 templatable.
- `id`는 ESPHome 위젯 id를 그대로 받는다.
- `set`의 속성은 한 번에 하나. 여러 속성은 action을 여러 번 쓴다.

### 3.2 C++ API

```cpp
namespace fx = esphome::lvgl_fx;

// Transition (값 전달)
fx::set_visible(obj, bool shown, Opt = {});
fx::set_opacity(obj, lv_opa_t opa, Opt = {});
fx::set_value(obj, float value, Opt = {});      // bar/arc/slider: 값, image: 회전(도), label: 숫자 (표시 형식은 transition `format`)
                                                // NaN → 즉시 최소값 (label은 "--")
fx::set_text(obj, const char *text, Opt = {});
fx::set_color(obj, lv_color_t color, Opt = {}); // label→글자색, image→recolor, 그 외→배경색
fx::set_x(obj, int32_t x, Opt = {});
fx::set_y(obj, int32_t y, Opt = {});
fx::set_rotation(obj, float degrees, Opt = {}); // transform 회전

// Effect (이름만)
fx::play(obj, EffectRef effect, Opt = {});
fx::stop(obj);
fx::stop(obj, EffectRef effect);

// 준비 완료
fx::start();

struct Opt {
  TransitionRef transition = nullptr;  // 이름 붙은 transition (없으면 속성 기본값)
  bool force = false;                  // 정책 억제 무시 (부팅 연출 등)
};
```

- `obj`는 `lv_obj_t *`와 ESPHome 위젯(`LvCompound`) 모두 받는다.
- 같은 값으로 다시 `set`하면 아무것도 하지 않는다 (진행 중이면 그대로 둠).
- `stop`은 움직임만 멈추고 **최종 상태로 즉시 정리**한다: transition은 목표 값, effect는 원래 값.

---

## 4. 정의 (`lvgl_fx:` 설정)

```yaml
lvgl_fx:
  # 4.1 모션 토큰
  motion:
    fast: 120ms
    normal: 200ms
    slow: 320ms
    intensity: 1.0          # effect의 크기(이동 거리, 확대율, 흔들림 폭)에 곱하는 배율

  # 4.2 정책
  suppress_when: !lambda return id(popup_open) || id(logs_visible);

  # 4.3 속성별 기본 transition (생략하면 아래 값)
  transitions:
    defaults:
      visible: { style: fade, duration: normal, curve: ease_out }
      opacity: { duration: normal }
      value:   { duration: normal, curve: ease_out }
      text:    { style: fade, duration: fast }
      color:   { duration: normal }
      position: { duration: normal, curve: ease_out }
      rotation: { duration: normal, curve: ease_out }
    # 이름 붙은 transition
    custom:
      - id: gauge_slow
        value: { duration: slow }
      - id: prnd_move
        position: { duration: 160ms }
        then: prnd_confirm        # 끝나면 같은 대상에 effect 재생
      - id: card
        visible: { show: slide_up, hide: fade, distance: 12 }
      - id: gear_roll
        text: { style: roll, distance: 14, duration: 140ms }

  # 4.4 기본 effect 조정 (선택)
  presets:
    pulse: { scale: 112% }
    flash: { color: white }

  # 4.5 사용자 effect
  effects:
    - id: prnd_confirm
      tracks:
        - flash: { property: bg_color, color: white, duration: 340ms, curve: ease_in }
    - id: heat
      cooldown: 2500ms
      tracks:
        - nudge: { y: -2, duration: 220ms }
        - flash: { property: image_recolor, amount: 25%, duration: 220ms }
    - id: warning_blink          # 해제될 때까지 반복
      repeat: infinite
      repeat_delay: 400ms
      tracks:
        - blink: { opacity: 35%, duration: slow }
```

### 4.1 모션 토큰 — [확정]
- `duration`에는 토큰 이름(`fast`) 또는 직접 값(`165ms`)을 쓴다.
- `intensity`는 effect의 크기에만 곱한다. transition의 목표 값에는 곱하지 않는다.
- `intensity` 변경은 다음 `play()`부터 적용되며 그때 실행 중인 모든 effect에 같이 적용된다 (값은 전역 하나, 인스턴스별 저장 없음).
- 런타임 변경 가능: `fx::motion().intensity = 0.6f;`

### 4.2 Transition 정의
- 속성별 기본값(`defaults`)이 있고, 이름 붙은 transition(`custom`)은 필요한 속성만 덮어쓴다.
- 필드: `style`, `duration`, `curve`, `delay`, `then`(완료 후 같은 대상에 재생할 effect), `policy`
- `then`은 transition이 끝나면 재생된다. 즉시 적용된 경우(duration 0, 정책으로 즉시 적용, NaN)도 끝난 것으로 보고 바로 재생한다
  (effect 자체의 정책은 그대로 적용). 이미 같은 값이라 아무것도 하지 않은 호출과 `stop`에서는 재생하지 않는다.
- 위젯에 transition을 묶어 두는 기능(호출 시 매번 지정하지 않도록)은 [보류].

### 4.2.1 Transition 스타일 — [확정]
스타일 = "기준값 전환"에 **0으로 돌아오는 진입 오프셋**을 곁들이는 모양. 6절의 오프셋 규칙을 그대로 따른다.

| 속성 | 스타일 | 모양 | 필드 (기본값) |
|---|---|---|---|
| `visible` | `fade` (기본) | 투명도만 | |
| | `slide_up` / `slide_down` / `slide_left` / `slide_right` | 투명도 + 위치 오프셋 distance → 0 | `distance` (12px) |
| | `zoom` | 투명도 + 크기 오프셋 from → 100% | `from` (90%) |
| | `none` | 즉시 | |
| `text` | `fade` (기본) | 새 텍스트가 `from` 투명도에서 100%로 | `from` (85%) |
| | `roll` | 새 텍스트가 아래/위에서 들어옴. **숫자가 커지면 아래에서, 작아지면 위에서** | `distance` (14px) |
| | `count` | 숫자가 이전 값에서 새 값까지 굴러감 (3.2의 `set_value`를 label에 쓸 때) | `format` (`%.0f`) |
| | `typewriter` | 한 글자씩 | `char_time` (20ms) — [보류: 2단계] |
| `value` `color` `opacity` `position` `rotation` | (없음) | 값 자체가 움직임 | |

- `visible`은 나타날 때와 사라질 때를 따로 줄 수 있다: `{ show: slide_up, hide: fade }`.
  사라질 때 오프셋은 0 → distance로 가고, HIDDEN이 된 뒤 0으로 되돌린다 (안 보이므로 티가 나지 않음).
- `roll`은 이전/새 텍스트의 앞쪽 숫자를 비교한다. 둘 중 하나라도 숫자가 아니면 `fade`처럼 동작한다.
- 스타일 구현은 재료(4.3)와 같은 계층이다. 새 스타일은 새 재료처럼 추가할 수 있다 (8.2).

### 4.3 Effect 정의 — [확정]
- effect = **track 목록**. track 하나 = 재료 하나(아래 표)가 속성 하나를 움직인다.
- **모든 track은 동시에 시작**하고, 순서는 track별 `delay`로 만든다 (CSS/영상 편집 타임라인과 같은 방식).
  effect의 길이 = 가장 늦게 끝나는 track.
- effect 필드: `tracks`, `repeat`, `repeat_delay`, `retrigger`, `when_suppressed`, `defer_timeout`, `cooldown`, `policy`, `then`

**재료 (track 종류)** — 기본 effect도 이것으로 정의되어 있다.

| 재료 | 필드 (기본값) | 움직이는 것 |
|---|---|---|
| `pulse` | `scale` (110%), `duration` (normal) | 크기: 1 → scale → 1 |
| `nudge` | `x`, `y` (px), `duration` (normal) | 위치 오프셋: 0 → (x, y) → 0 (기본 effect `nudge_*`는 4px) |
| `shake` | `distance` (2px), `cycles` (2), `axis` (x), `duration` (fast) | 위치 오프셋: 좌우 왕복 |
| `flash` | `property` (위젯 종류별), `color` (white), `amount` (100%), `duration` (slow) | 색: 강조색 → 원래 색 |
| `glow` | `opacity` (50%), `duration` (slow) | outline 투명도: opacity → 0 |
| `blink` | `opacity` (30%), `duration` (normal) | 투명도: 1 → opacity → 1 |

공통 필드: `delay`, `curve`.

**기본 제공 effect**: `pulse`, `shake`, `nudge_up`, `nudge_down`, `nudge_left`, `nudge_right`, `flash`, `glow`, `blink`
(각각 위 재료 하나로 된 effect). `presets:`로 수치만 조정할 수 있다.

### 4.4 Effect 옵션 — [확정]

| 옵션 | 값 (기본값 굵게) | 의미 |
|---|---|---|
| `retrigger` | **`restart`** / `ignore` | 재생 중 다시 호출: 처음부터 다시 / 무시 |
| `when_suppressed` | **`skip`** / `defer` | 억제 중 호출: 버림 / 하나만 보관했다가 억제가 풀리면 재생 |
| `defer_timeout` | **3s** | `defer`로 보관한 것의 유효 시간 |
| `cooldown` | **0** | 같은 대상에서 이 effect의 최소 재생 간격. 대상의 **직전 1회성 effect가 같은 effect일 때만** 적용 (A→B→A는 A를 다시 재생) |
| `policy` | **`normal`** / `always` | `always`면 `suppress_when`을 무시 |

| `repeat` | **1** / N / `infinite` | 반복 횟수. 한 번 = 모든 track이 끝날 때까지 |
| `repeat_delay` | **0** | 반복 사이 쉬는 시간 |

- 전역 기본값은 `lvgl_fx: effect_defaults:`에서 바꿀 수 있다. 우선순위: effect 정의 > 전역 기본값.
- 호출에서는 바꿀 수 없다 (원칙 1). 예외는 `force` 하나.

### 4.4.1 반복 effect — [확정]
- `repeat: N`은 일반 effect와 같다. 길이만 N배다.
- `repeat: infinite`(이하 **루프**)는 "켜져 있는 상태"로 취급한다. `play`로 켜고 `stop`으로 끈다.
  - 이미 돌고 있는 루프를 다시 `play`하면 **아무것도 하지 않는다** (`retrigger` 무시). 같은 호출을 반복해도 안전하다.
  - `stop`하면 반복 중간이라도 오프셋을 원래 값으로 즉시 되돌린다.
  - `cooldown`, `defer_timeout`은 루프에 적용하지 않는다.
  - 억제·숨김 중 동작은 5절 표의 "루프" 열을 따른다.
  - `reduced_motion`이면 움직이지 않는다. 루프가 켜져 있다는 사실은 기억한다.
- `then`은 루프에 쓸 수 없다 (끝나지 않으므로). 설정 검증에서 오류.

### 4.5 이름 참조 방식 — [확정]
transition·effect의 이름은 **ESPHome `id`**로 선언한다.
- YAML: `effect: heat` → 존재하지 않으면 **설정 검증 단계에서 오류**.
- C++: `fx::play(obj, id(heat))` → 오타면 **컴파일 오류**.
- 기본 effect는 YAML에서 예약어(`pulse`), C++에서 상수(`fx::PULSE`).

---

## 5. 정책 — [확정]

Transition과 1회성 effect는 시작 시점에 한 번 판정한다 (프레임마다 아님).
루프는 켜져 있는 동안 계속 판정 대상이다 (아래 `loop()` 확인).

| 순서 | 조건 | Transition | Effect (1회, N회) | 루프 (`infinite`) |
|---|---|---|---|---|
| 1 | `enabled == false` 또는 `reduced_motion` | 즉시 적용 | 버림 | 켜짐만 기억 |
| 2 | ready 전 (`lvgl_fx.start` 전) | 즉시 적용 | 버림 | 켜짐만 기억, ready 되면 시작 |
| 3 | 대상이 안 보임 (자신 또는 부모가 HIDDEN) | 즉시 적용 | 버림 | 원래 값으로 멈춤, 다시 보이면 재개 |
| 4 | `suppress_when` true (`policy: always` 제외) | 즉시 적용 | `when_suppressed` 따름 | 원래 값으로 멈춤, 풀리면 재개 |
| 5 | `cooldown` 미충족 | — | 버림 | — |

- `force: true`는 1–5를 모두 무시한다.
- `defer` 보관분은 4번으로 억제됐을 때만 생긴다.
- `loop()`는 확인할 것이 있을 때만 일한다. 없으면 바로 반환한다.
  - `defer` 보관분: 억제가 풀렸는지 → 재생 또는 시간 초과로 버림
  - 켜져 있는 루프: 돌고 있으면 억제·숨김이 **시작됐는지**, 멈춰 있으면 **풀렸는지** → 멈춤/재개
  - 재개는 처음 위상부터 시작한다 (멈춘 위치를 기억하지 않음 — 저장 비용과 어색한 중간 시작을 피함).
- `suppress_when` lambda는 **메인 루프 한 바퀴에 최대 한 번** 평가하고 결과를 캐시한다 (7.4).

---

## 6. 내부 구조: 기준값과 효과 오프셋 분리 — [확정]

### 문제
같은 속성을 transition과 effect가 동시에 움직이면 결과가 틀어진다.
예: `set_color`로 색이 바뀌는 중에 `flash`가 같은 색 속성에 걸리면, flash가 끝날 때 **중간색에 멈춘다** (원칙 3 위반).
hide 페이드아웃 도중 `blink`가 걸려도 같은 문제가 생긴다.

### 해결: 모든 속성을 "기준값 + 오프셋"으로 나눈다
- **기준값은 Transition만** 바꾼다. 화면에 적용되는 값 = 기준값 ⊕ 오프셋들.
- **오프셋은 항상 0(원래 값)으로 끝난다.** 이것이 핵심 불변식이다. 오프셋을 만드는 곳은 셋이다:
  1. 재생 중인 effect (1회/N회)
  2. 재생 중인 루프
  3. Transition 스타일의 진입 오프셋 (4.2.1)
- 오프셋은 저장하지 않고 **각자의 진행률에서 매 프레임 계산**한다. 어느 하나가 끝나거나 멈추면 그 기여가 빠질 뿐이라
  어떤 순서로 겹쳐도 마지막에는 기준값만 남는다.

| 속성 | 기준값 (Transition) | 오프셋 | 합성 |
|---|---|---|---|
| 위치 | `x`, `y` (LVGL) | nudge, shake, slide | 오프셋 합 → `translate_x/y` |
| 투명도 | 기준 opa | blink, fade 스타일 | 곱 → `opa` |
| 크기 | 기준 scale | pulse, zoom | 곱 → `transform_scale` |
| 회전 | 기준 회전 | (1단계 없음) | 합 → `transform_rotation` |
| 색 | 기준 색 | flash (강조색, 강도) | 혼합 |
| outline | 위젯의 `outline_opa` (처음 다룰 때 읽음; 보이지 않게 하려면 0으로 설정) | glow | 최대값 → `outline_opa` |
| 위젯 값, 텍스트 | 기준값 | text 스타일(roll 등)은 위치·투명도 오프셋으로 | 그대로 |

- 위치 기준값은 LVGL의 `x`/`y`를 그대로 쓴다. 오프셋은 `translate`에 합쳐서 쓴다.
  따라서 **사용자가 `translate`를 직접 쓰는 위젯에는 위치 오프셋 효과를 쓰지 않는다** (README에 명시).

### 소유권 규칙 — [확정]
- fx로 다루기 시작한 속성은 **이후 fx로만 바꾼다.** 처음 다룰 때 위젯의 현재 값을 기준값으로 읽어 온다.
- 예외: 위젯 값(bar/arc/slider), 텍스트, 위치는 `set` 할 때마다 위젯에서 현재 값을 다시 읽으므로
  외부에서 바꿔도 깨지지 않는다.
- 같은 속성을 transition 둘이 움직이면(예: `set_opacity` 중 `set_visible(false)`) **나중 것이 이기고,
  현재 위치에서 이어간다.**

### 보임/숨김 규칙 — [확정]
- `set_visible(true)`: HIDDEN 플래그를 **즉시** 해제하고 기준 opa를 현재 값 → 100%로.
- `set_visible(false)`: 기준 opa를 현재 값 → 0으로, **끝나면** HIDDEN 설정.
- 숨기는 도중 다시 `set_visible(true)`가 오면 현재 opa에서 되돌아온다. 이전 숨김의 완료 콜백은 무효가 된다
  (대상별 세대 번호로 구분).
- `set_opacity`와 `set_visible`은 같은 기준 opa를 쓴다. `set_visible(true)`의 목표는 "마지막으로 설정된 opacity"(기본 100%).

---

## 7. 실행 구조와 최적화 — [확정]

### 7.1 실행 단위: 인스턴스 하나 = `lv_anim` 하나
- **effect 하나를 재생할 때 track 수와 상관없이 `lv_anim`은 하나**만 만든다.
  `lv_anim`은 0 → effect 전체 길이(ms)를 선형으로 진행하고, 각 track은 자기 `delay`/`duration`/`curve` 구간을
  이 진행 시간에서 잘라 계산한다. (Colorado `icon_frame`이 이미 이 방식이다.)
  - 장점: track이 3개여도 heap 할당·프레임 콜백·삭제가 한 번. `stop`과 `retrigger`가 단순해진다.
  - 반복은 `lv_anim`의 `repeat_count`/`repeat_delay`를 그대로 쓴다.
- **transition은 (대상, 속성)마다 `lv_anim` 하나.** 스타일의 진입 오프셋도 같은 `lv_anim`의 진행률로 계산한다.
- `lv_anim`의 `var`는 슬롯 주소이고, 인스턴스마다 `exec_cb`가 다르다 (P2 구현). `(var, exec_cb)` 쌍이 키이므로 겹치지 않고,
  대상 삭제 시 `lv_anim_delete(slot, nullptr)` 한 번으로 모두 정리된다.

### 7.2 대상당 동시 재생 수 — [확정]
대상 하나에 **1회성 effect 1개 + 루프 1개**가 동시에 돌 수 있다.
- 1회성 effect를 재생 중에 다른 1회성 effect를 `play`하면 기존 것은 끝난 것으로 처리(오프셋 0)하고 새 것을 시작한다.
- 루프도 같다. 다른 루프를 `play`하면 교체된다.
- 이유: 경고 루프(깜빡임)가 도는 위젯에 값 변화 펄스가 한 번 더 걸리는 경우는 흔하지만,
  1회성 효과 둘이 동시에 필요하면 둘을 합친 effect를 정의하는 것이 맞다.

### 7.3 대상별 슬롯
**필요할 때만 만든다.** 아래 경우는 슬롯 없이 처리한다:
위젯 값(bar/arc/slider)·텍스트·위치의 transition에서 스타일이 없고, 같은 대상에 effect도 없을 때.

```cpp
struct Slot {                          // 68 바이트 (P2 실측, 32비트 타깃)
  lv_obj_t *obj;                       // 4  대상
  uint16_t generation;                 // 2  visible 세대 (6절)
  uint8_t  props;                      // 1  fx가 소유한 합성 속성 비트
  uint8_t  flags;                      // 1  루프 켜짐, 루프 멈춤, defer 있음 …
  uint8_t  base_opa, visible_opa;      // 2
  uint16_t base_scale;                 // 2
  int16_t  base_rotation;              // 2
  lv_color_t base_color;               // 3 (+1)
  uint8_t  base_outline_opa;           // 1  glow의 outline 기준 투명도 (P2 추가)
  Instance one_shot, loop, style;      // 3 × 8 = 24  {정의 포인터, 진행 ms, 상태}
  uint32_t last_play_tick;             // 4  cooldown
  const EffectDef *deferred;           // 4  defer 보관분
  uint32_t deferred_tick;              // 4  보관 시각 (defer_timeout)
  const TransitionDef *opa_then;       // 4  진행 중인 투명도/visible transition의 `then` (P2 추가: 그 anim의 user_data는 세대 번호)
};
```
- 오프셋은 저장하지 않는다 (6절: 진행률에서 계산). 그래서 슬롯이 작다.
- 크기: 슬롯 68바이트(P2 실측), 대상 32개 ≈ 2.2 KB. `max_targets`로 조정. Tab5처럼 internal RAM이 빠듯하면
  `slots_in_psram: true`로 PSRAM에 둘 수 있다 (프레임당 접근 수가 적어 속도 영향 미미).
- 정확한 크기는 구현 후 측정해 이 문서에 기록한다.
- 찾기: 슬롯 수가 작으므로 **선형 탐색 + 마지막 찾은 슬롯 캐시**. 프레임 콜백은 `var`로 슬롯을 바로 알기 때문에 탐색하지 않는다.
- 정리: 대상 삭제 시 `LV_EVENT_DELETE`로 슬롯과 `lv_anim`을 정리한다.
  가득 차면 해당 호출은 즉시 적용으로 대체하고 경고 로그 한 번.

### 7.4 호출 경로의 빠른 반환
- `set`: 목표가 현재 기준값과 같고 진행 중인 transition이 없으면 **슬롯 탐색 전에** 반환한다.
  같은 값이 1초에 수십 번 들어오는 센서 갱신에서 비용이 거의 없다.
- `set` 중 진행 중인 transition의 목표와 같으면 아무것도 하지 않는다 (재시작 없음).
- `suppress_when`은 메인 루프 한 바퀴에 최대 한 번 평가하고 캐시한다.
- 정책 1–3번(비활성, ready 전, 숨김)은 lambda 평가 전에 판정한다.

### 7.5 프레임 비용
- 프레임 콜백은 정수 연산만. 곡선은 LVGL의 `lv_anim_path_*`와 같은 정수 근사를 쓴다 (`float` 없음).
- 합성은 `props` 비트에 있는 속성만 다시 쓴다.
- **렌더링이 비싼 속성 주의**: `transform_scale`/`transform_rotation`과 `opa < 255`는 LVGL이 레이어를 따로 그린다.
  큰 컨테이너에 걸면 프레임이 떨어진다. 화면 면적의 25% 이상인 대상에 이 속성을 처음 걸 때 경고 로그 한 번.
  README에 "큰 영역은 translate/색 위주로" 가이드를 둔다.

### 7.6 코드 크기와 정의 데이터
- transition/effect 정의는 코드 생성 시 **`const` 테이블**이 되어 flash(rodata)에 놓인다. RAM을 쓰지 않는다.
- `then` 체인, 스타일, 재료 참조는 테이블 안의 포인터라 런타임 저장이 없다.
- 위젯 종류별 코드(`bar`, `arc`, `slider`, `image`, `label`)는 `LV_USE_*`로 조건부 컴파일.
- 재료와 스타일은 설정에서 실제로 쓰인 것만 `USE_LVGL_FX_<NAME>` define으로 켠다.
  C++ 상수(`fx::PULSE` 등)로 참조되는 기본 effect는 설정에서 알 수 없으므로 항상 포함한다 (각각 수십 바이트).

---

## 8. 확장

### 8.1 확장 지점 (사용자 쪽)
정의로 표현할 수 없는 경우를 위한 통로. 예외는 먼저 여기로 흡수한다.

| 확장 지점 | 용도 |
|---|---|
| **사용자 채널** | `fx::tween(var, setter, from, to, {duration, curve, delay, on_done})` — 임의 대상, 임의 setter. 정책은 그대로 적용 |
| **사용자 재료** | `fx::register_track("name", fn)` — 새 track 종류를 C++로 추가하고 YAML `effects:`에서 사용 — [보류: 2단계] |
| **정책 lambda** | `suppress_when` |

### 8.2 컴포넌트에 효과를 추가하는 방법 (개발 쪽)

| 추가하려는 것 | 방법 | 건드리는 곳 |
|---|---|---|
| 기존 재료를 조합한 새 effect | `effects:`에 track 나열 | YAML만 |
| 기본 effect 추가 | 재료 하나로 된 정의를 기본 테이블에 추가 | `builtins` 테이블 |
| 새 재료 (예: `wobble`, `bounce`) | 재료 함수 하나 + YAML 스키마 | `tracks.*`, `__init__.py` |
| 새 transition 스타일 (예: `flip`, `decode`) | 스타일 함수 하나 + YAML 스키마 | `styles.*`, `__init__.py` |
| 새 속성 (예: 너비, 그림자, 선 진행률) | 기준값 필드 + 합성 규칙 + `set_*` 함수 + YAML `set` 필드 | `props.*`, 슬롯, API |

**재료와 스타일 함수의 규칙** (이것만 지키면 정책, 충돌, 중단 규칙이 자동으로 적용된다):
1. 입력: 정의 데이터, 진행 시간(ms), 모션 토큰. 출력: 오프셋 기여분. 위젯을 직접 건드리지 않는다.
2. 시작 지연(`delay`) 전과 끝에서부터 **오프셋 기여분은 0**이어야 한다 (6절 불변식). pulse/nudge/shake/blink는 구간 시작도 0이고,
   flash/glow는 구간 시작에서 최대값으로 시작해 0으로 줄어든다. 진입 스타일은 끝에서 0, 퇴장은 HIDDEN 후 0.
3. 정수 연산만, 할당 없음.

새 속성이 늘면 슬롯이 커진다. 속성이 많이 늘어나면 "쓰는 속성만 저장"하는 가변 구조로 바꿀 수 있으며, 이는 내부 구현이라 API에 영향이 없다.

---

## 9. 1단계 범위

**호출**: `set`(visible, opacity, value, text, color, x, y, rotation), `play`, `stop`, `start` — YAML과 C++ 모두
**정의**: 모션 토큰, `transitions:`(defaults, custom, then), `presets:`, `effects:`(tracks, repeat, 옵션 전부)
**재료**: pulse, nudge, shake, flash, glow, blink
**스타일**: visible(fade, slide_*, zoom, none), text(fade, roll, count)
**내부**: 정책, 기준값/오프셋 분리, 인스턴스당 `lv_anim` 하나, 필요할 때만 만드는 슬롯, 빠른 반환, 사용자 채널

**제외 [보류]**: 위젯-transition 묶기, 사용자 재료 등록, timeline, typewriter/decode 스타일,
선 그리기, stagger(여러 대상 순차)

### 9.1 구현 페이즈 — [확정]
1단계를 5개 페이즈로 나눈다. 페이즈마다 구현 → 리뷰(필수 지적 0건까지) → PR → squash merge.
각 페이즈는 `tests/components/lvgl_fx/test.esp32-idf.yaml`이 컴파일되는 상태로 끝난다.

| 페이즈 | 범위 |
|---|---|
| **P1 core** | 컴포넌트 골격, 최소 YAML(`motion`, `suppress_when`, `max_targets`, `slots_in_psram`, `lvgl_fx.start`), 속성 합성(6절), 슬롯(7.3), 정책(5절, transition 열), 빠른 반환(7.4), `set_*` 전부(기본 transition, visible/text는 `fade`), visible 세대 규칙, `stop`, 사용자 채널 `fx::tween`, C++ 내장 transition 테이블 형식 |
| **P2 effect** | runner(7.1), 재료 6종, 기본 effect + `presets`(C++ 테이블), effect 옵션 전부, 반복·루프(4.4.1, 5절 루프 열), 1회성 1 + 루프 1, `defer`, `cooldown`, transition `then` |
| **P3 스타일** | visible `slide_*`/`zoom`/`none`, `show`/`hide` 분리, text `roll`/`count`, label `set_value` + `format` |
| **P4 YAML** | `transitions:`/`presets:`/`effects:`/`effect_defaults:` 코드 생성(const 테이블), ESPHome id 참조, 설정 검증(예: 루프에 `then` 금지), action `set`/`play`/`stop`, `USE_LVGL_FX_<NAME>` define |
| **P5 마무리** | 데모 YAML(lambda 없이 전 기능), 테스트 보강, README, `components/README.md` 표, 슬롯 크기 실측 기록 |

Colorado 적용은 P5 이후 별도 페이즈로 진행한다.

---

## 10. 예외 흡수 규칙

Colorado 적용 중 구조에 맞지 않는 상황이 생기면:

1. **먼저 확장 지점(8.1)으로 해결**하고 코드는 Colorado 쪽에 둔다.
2. **core 변경은 두 번째 사용처(데모 또는 다른 대시보드)도 같은 것이 필요할 때만** 한다.
3. 확장 지점으로도 안 풀리면 설계 결정이 틀렸다는 신호다. 땜질하지 말고 이 문서를 고친 뒤 진행한다.
4. 흡수한 예외는 12절에 기록한다.

---

## 11. 검증

### 11.1 Colorado 표현 가능성 (설계 검증용)
✓ = 정의+호출로 표현, △ = 확장 지점 사용

| Colorado 애니메이션 | 표현 |
|---|---|
| 팝업/아이콘 표시·숨김 | ✓ `set visible` |
| splash 페이드아웃 | ✓ `set opacity` + `force` |
| 게이지 bar, 속도 arc | ✓ `set value` (상승/하강 시간이 다르면 transition 두 개를 Colorado가 골라 호출) |
| 기울기 이미지 회전 | ✓ `set value` (image) |
| pose 컨테이너 회전 | ✓ `set rotation` |
| 값 텍스트 교체 페이드 | ✓ `set text` |
| PRND 글자 펄스 | ✓ `play pulse` |
| PRND 선택바 이동 → 확인 flash | ✓ `set x` + transition `then` |
| 기어 숫자 위/아래 | ✓ `set text` + 스타일 `roll` (방향 자동) |
| outline 깜빡임 | ✓ `play glow` |
| 아이콘 heat/cold/drop/dry/grow | ✓ 사용자 effect 5개 + `cooldown`. 색→효과 선택은 Colorado |
| 아이콘 간 최소 간격 (230ms) | △ Colorado 쪽 판단, 두 번째 사용처에서도 필요하면 core로 |
| 경고 영역 pulse (지연 포함) | ✓ 사용자 effect (`blink`/`flash` + `delay`) |
| readout 복구 tint | ✓ `play flash` (text_color) |
| scan trace 선 그리기 | △ 사용자 채널 |
| 부팅 타이핑 | △ Colorado에 유지 (2단계 typewriter/decode 스타일 + timeline 후 이전) |
| (Colorado에 없음) 경고 지속 깜빡임 | ✓ 루프 effect — 데모에서 검증 |

### 11.2 Colorado와 무관한 데모 YAML
두 번째 사용처 역할. **lambda 없이 YAML만으로** 모든 1단계 기능을 쓸 수 있는지가 범용성 기준.

### 11.3 빌드, 실행
- `tests/`에 컴파일 테스트 YAML (ESPHome 2026.11.0)
- Colorado 적용 전/후 internal RAM 비교
- esp-emu 부팅 확인, 실기기 육안 비교

---

## 12. 예외 기록

(Colorado 적용 중 작성)

---

## 13. 파일 구조

```
components/lvgl_fx/
  __init__.py      설정 스키마, 정의 테이블 코드 생성, action 등록 (P1: 최소 스키마)
  lvgl_fx.h/.cpp   컴포넌트, 정책, 슬롯 풀, 공개 API, transition (set_*)
  effects.h        곡선, effect/track 정의 형식(const 테이블), 기본 effect 상수, 재료 계산 선언
  tracks.cpp       재료 계산 (pulse, nudge, shake, flash, glow, blink), 기본값/preset 해석
  runner.cpp       effect 실행: play/stop, 1회성/루프 인스턴스, 정책, defer, loop() 처리
  props.h/.cpp     속성별 기준값/오프셋 합성 (LV_USE_* 조건부)
  builtins.cpp     기본 effect, 기본 transition 테이블
  automation.h     YAML action
  styles.h/.cpp    transition 스타일 (P3 예정)
  README.md        (P5 예정)
  DESIGN.md
```
