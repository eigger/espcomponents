# 현대 Imazu 월패드

현관문 프리셋은 실기기 검증 전입니다. 두 구성은 UART 통신 속도가 다릅니다.

| 파일 | 용도 |
| --- | --- |
| [hyundai_imazu.yaml](hyundai_imazu.yaml) | 월패드 |
| [hyundai_door.yaml](hyundai_door.yaml) | 현관문 — 실기기 검증 전 |

## 구성

[월패드 기본 구성](../README.md#기본-구성)에 보드·네트워크 설정을 준비하고
아래 패키지 중 하나를 선택하세요.

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/wallpad/hyundai/hyundai_imazu.yaml
```

현재 UART 설정은 **월패드 9600 / 8N1, 현관문 3880 / 8N1**, RX `GPIO22` / TX `GPIO19`입니다.
보드와 RS-485 모듈 조합에 맞는 핀은 [공통 핀맵](../README.md#하드웨어와-uart-핀)을
참고하세요. 실제 단지의 수신 로그와 YAML의 방·장치 주소를 비교한 뒤 사용하세요.
