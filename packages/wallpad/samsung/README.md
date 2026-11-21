# 삼성 SDS 월패드

## 파일

| 파일 | 용도 | UART |
| --- | --- | --- |
| [`samsung_sds.yaml`](samsung_sds.yaml) | 월패드 | 9600 / 8E1 |
| [`samsung_sds_door.yaml`](samsung_sds_door.yaml) | 기존 현관 인터폰 단독 구성 | 9600 / 8E1 |
| [`samsung_sds_shared.yaml`](samsung_sds_shared.yaml) | 같은 회선의 월패드 연동과 가상 인터폰 | 9600 / 8E1 |

## 사용하기

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: latest
    files:
      - packages/device_base.yaml
      - packages/wallpad/samsung/samsung_sds.yaml
```

`substitutions`·`esp32` 등 나머지는 [기본 구성](../README.md#기본-구성)과 같습니다.
보드·RS-485 모듈이 Atom Lite + Atomic RS485 Base가 아니라면 [하드웨어와 UART 핀](../README.md#하드웨어와-uart-핀)에 따라 `uart:` 핀을 추가하세요.

단독 월패드와 공용 패키지는 가변 길이 수신을 지원하는 UARTEx `latest`를 사용합니다. `samsung_sds_component_ref`로 이 기능을 포함한 버전을 지정할 수 있습니다.

## 월패드와 가상 인터폰을 같은 회선에서 사용하기

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: latest
    files:
      - packages/device_base.yaml
      - packages/wallpad/samsung/samsung_sds_shared.yaml
```

공용 패키지 하나에 월패드 엔티티와 가상 인터폰이 함께 들어 있습니다. 한 UART에서 월패드 장치의 응답과 가상 인터폰에 온 요청을 구분합니다. `A4` 주소가 비어 있는 회선에서 사용하세요. 가상 인터폰은 스캔과 반복 폴링마다 응답하고, 세대·공동현관의 벨 시작과 종료를 각각 센서에 반영합니다. 문 열림 제어는 범위에 포함되지 않습니다.

가상 인터폰 응답은 즉시 전송합니다. 일반 월패드 제어 명령은 기본 `tx_delay: 5ms` 이상의 유휴 구간에 전송합니다.

## 제보하기

구성이 잘 동작했거나, 구성을 추가했거나, 오류가 발생했다면 [이슈](https://github.com/eigger/espcomponents/issues/new) 또는 [토론](https://github.com/eigger/espcomponents/discussions/new?category=q-a)으로 제보해 주세요. 제조사·단지(모델), 사용한 파일, 가능하면 수신 로그를 함께 적어 주세요.
