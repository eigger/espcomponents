# BMM150

Bosch BMM150 3축 지자기 센서. 원시 XYZ와 차량용 방위각(heading)을 제공합니다.

M5Stack Unit GNSS는 주소 **0x10**. 나침반 용도라면 `update_interval`을 **200ms~1s**로 두는 것을 권장합니다.

## 옵션

| 키 | 기본값 | 설명 |
|---|---|---|
| `address` | `0x10` | I2C 주소 (0x10~0x13, CSB/SDO 스트랩) |
| `update_interval` | `60s` | 폴링 주기. 나침반은 200ms~1s |
| `magnetic_field_x/y/z` | — | 지자기 (µT, Bosch 정수 보상. **실측 미검증**) |
| `heading` | — | 진북 방위각 0~360°. 캘리브레이션 전에는 `unknown` |
| `accel_x_id` / `accel_y_id` / `accel_z_id` | — | 틸트 보정용 가속도 센서 3개. 미지정 시 평면 공식 |
| `declination` | `0` | 자기 편각(도). 한국은 약 **-8**(서편). 기본값을 지역에 맞추지 않음 |
| `soft_iron` | `true` | 캘리브레이션 시 축별 스케일 보정 |
| `mag_axes` | `[x, y, z]` | 지자기 축 리매핑 (`x`,`y`,`z`,`-x`,`-y`,`-z`). **기판 정렬 미검증** |
| `accel_axes` | `[x, y, z]` | 가속도 축 리매핑. **기판 정렬 미검증** |
| `on_calibration_finished` | — | 캘리브레이션 종료 트리거. `success`(bool) |

## 캘리브레이션

차량은 철판·스피커 때문에 하드아이언 오프셋이 큽니다. 보정 없이 heading은 내지 않습니다.

1. 센서를 설치 위치에 고정한 채 아래 액션을 실행합니다.
2. `duration` 동안 기기를 **8자**로 천천히 회전합니다 (세 축이 모두 움직이게).
3. 축별 min/max 차이(delta)가 **20 µT 미만**이면 저장하지 않고 `success=false`.
4. 통과하면 NVS에 저장되어 재부팅 후에도 유지됩니다.

```yaml
on_...:
  - bmm150.calibrate:
      id: mag
      duration: 30s
```

## 차량 설치

스피커 자석, 철판, 대전류 배선에서 가능한 한 떨어뜨리십시오. GNSS 모듈을 대시보드 철판에 붙이면 오프셋이 수십 µT로 커집니다.

## 축 정렬 · 편각

틸트 보정 공식은 mag/accel의 X/Y/Z가 같은 방향을 가리킨다고 가정합니다. M5 Unit GNSS의 BMM150과 BMI270은 기판에서 축이 다를 수 있으니, 실측 후 `mag_axes` / `accel_axes`로 맞추십시오.

진북이 필요하면 `declination`을 넣습니다. 서울은 대략 `-8`.

## 예시

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ bmm150 ]

i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

sensor:
  - platform: bmm150
    id: mag
    address: 0x10
    update_interval: 500ms
    magnetic_field_x:
      name: "MAG X"
    magnetic_field_y:
      name: "MAG Y"
    magnetic_field_z:
      name: "MAG Z"
    heading:
      name: "Heading"
    accel_x_id: id_gnss_bmi270_accel_x
    accel_y_id: id_gnss_bmi270_accel_y
    accel_z_id: id_gnss_bmi270_accel_z
    declination: -8
    mag_axes: [x, y, z]
    accel_axes: [x, y, z]
    on_calibration_finished:
      - logger.log:
          format: "BMM150 calibration %s"
          args: ['success ? "ok" : "failed"']
```
