# CVnet 월패드

> [!WARNING]
> **작성 중 · 실기기 검증 전**입니다. 패킷 전체 규격과 모델별 호환성은 확인되지 않았습니다.

## 파일

| 파일 | 용도 | UART | 상태 |
| --- | --- | --- | --- |
| [`cvnet.yaml`](cvnet.yaml) | 월패드 | 9600 / 8N1 | 작성 중 · 실기기 검증 전 |

## 사용하기

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/wallpad/cvnet/cvnet.yaml
```

`substitutions`·`esp32` 등 나머지는 [기본 구성](../README.md#기본-구성)과 같습니다.
보드·RS-485 모듈이 Atom Lite + Atomic RS485 Base가 아니라면 [하드웨어와 UART 핀](../README.md#하드웨어와-uart-핀)에 따라 `uart:` 핀을 추가하세요.
