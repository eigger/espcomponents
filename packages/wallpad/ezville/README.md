# 이지빌 월패드

조명 프레임 등 세부 패킷은 YAML 주석을 참고하세요.

## 파일

| 파일 | 용도 | UART |
| --- | --- | --- |
| [`ezville.yaml`](ezville.yaml) | 월패드 | 9600 / 8N1 |

## 사용하기

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: latest
    files:
      - packages/device_base.yaml
      - packages/wallpad/ezville/ezville.yaml
```

`substitutions`·`esp32` 등 나머지는 [기본 구성](../README.md#기본-구성)과 같습니다.
보드·RS-485 모듈이 Atom Lite + Atomic RS485 Base가 아니라면 [하드웨어와 UART 핀](../README.md#하드웨어와-uart-핀)에 따라 `uart:` 핀을 추가하세요.

## 참고

- [KS X 4506 홈네트워크 프로토콜](https://standard.go.kr/KSCI/ksNotification/getKsNotificationView.do?ntfcManageNo=2016-00079&menuId=921&topMenuId=502)

## 제보하기

구성이 잘 동작했거나, 구성을 추가했거나, 오류가 발생했다면 [이슈](https://github.com/eigger/espcomponents/issues/new) 또는 [토론](https://github.com/eigger/espcomponents/discussions/new?category=q-a)으로 제보해 주세요. 제조사·단지(모델), 사용한 파일, 가능하면 수신 로그를 함께 적어 주세요.
