# 삼성 SDS 월패드

현관문 프리셋은 실기기 검증 전입니다. 월패드와 현관문은 프레임 설정이 다릅니다.

| 파일 | 용도 |
| --- | --- |
| [samsung_sds.yaml](samsung_sds.yaml) | 월패드 |
| [samsung_sds_door.yaml](samsung_sds_door.yaml) | 현관문 — 실기기 검증 전 |

## 구성

[월패드 기본 구성](../README.md#기본-구성)에 보드·네트워크 설정을 준비하고
아래 패키지 중 하나를 선택하세요.

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/wallpad/samsung/samsung_sds.yaml
```

현재 UART 설정은 **9600 / 8E1**, RX `GPIO22` / TX `GPIO19`입니다.
보드와 RS-485 모듈 조합에 맞는 핀은 [공통 핀맵](../README.md#하드웨어와-uart-핀)을
참고하세요. 실제 단지의 수신 로그와 YAML의 방·장치 주소를 비교한 뒤 사용하세요.
