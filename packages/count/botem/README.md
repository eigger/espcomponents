# 보탬 CSM505 인원 카운터

UART 이벤트를 `uartex`로 받아 `People Count`를 증감하는 예제입니다.
**실기기 검증 전** 구성이므로 설치 환경의 패킷과 비교해야 합니다.

## 구성

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/count/botem/botem_csm505.yaml
```

[공통 설정](../../README.md#common-device-setup)에 따라 `name`, `friendly_name`,
Wi-Fi/OTA secrets를 추가하세요. 이 패키지는 보드를 지정하지 않으므로
사용하는 ESP32의 `esp32:` 설정도 필요합니다.

| 항목 | 현재 설정 |
| --- | --- |
| UART | `9600`, 8N1 |
| RX / TX | `GPIO22` / `GPIO19` |
| 프레임 | `^` 시작, `\r` 종료 |
| 증가 / 감소 | `[001]` / `[002]` |
| 인원 수 상한 | `30` |

보드에 맞게 UART 핀을 변경하고, 수신 로그에서 실제 증가·감소 패킷을 확인하세요.

[CSM505 제품 안내](http://botem-e.com/kor/product/menu_00.html?pd_idx=24&tmode=view&code=2) ·
[구매 링크](https://botemarket.com/kor/shop/shop.php?pmode=view&prodSeq=56&cg_code1=2) ·
[uartex](../../../components/uartex)
