# 삼성 SDS 월패드

## 파일

| 파일 | 용도 | UART |
| --- | --- | --- |
| [`samsung_sds.yaml`](samsung_sds.yaml) | 월패드 | 9600 / 8E1 |
| [`samsung_sds_door.yaml`](samsung_sds_door.yaml) | 현관문 | 9600 / 8E1 |

## 사용하기

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/wallpad/samsung/samsung_sds.yaml
```

`substitutions`·`esp32` 등 나머지는 [기본 구성](../README.md#기본-구성)과 같습니다.
보드·RS-485 모듈이 Atom Lite + Atomic RS485 Base가 아니라면 [하드웨어와 UART 핀](../README.md#하드웨어와-uart-핀)에 따라 `uart:` 핀을 추가하세요.

> [!NOTE]
> 현관문은 월패드와 다른 RS-485 라인을 쓰므로 **별도 장치**로 만들고, 위 마지막 줄을 현관문 파일로 바꾸세요.

## 제보하기

구성이 잘 동작했거나, 구성을 추가했거나, 오류가 발생했다면 [이슈](https://github.com/eigger/espcomponents/issues/new) 또는 [토론](https://github.com/eigger/espcomponents/discussions/new?category=q-a)으로 제보해 주세요. 제조사·단지(모델), 사용한 파일, 가능하면 수신 로그를 함께 적어 주세요.
