# CVnet 월패드

**작성 중 / 실기기 검증 전**입니다. 패킷 전체 규격과 모델별 호환성은 확인되지 않았습니다.

| 파일 | 용도 |
| --- | --- |
| [cvnet.yaml](cvnet.yaml) | 월패드 초안 |

## 구성

[월패드 기본 구성](../README.md#기본-구성)에 보드·네트워크 설정을 준비하고
아래 패키지 중 하나를 선택하세요.

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/wallpad/cvnet/cvnet.yaml
```

현재 UART 설정은 **9600 / 8N1**, RX `GPIO22` / TX `GPIO19`입니다.
보드와 RS-485 모듈 조합에 맞는 핀은 [공통 핀맵](../README.md#하드웨어와-uart-핀)을
참고하세요. 실제 단지의 수신 로그와 YAML의 방·장치 주소를 비교한 뒤 사용하세요.
