# Verta 루버

통신 방식에 따라 프리셋을 선택하는 루버 제어 구성입니다.

| 파일 | 구성 / 선행 조건 |
| --- | --- |
| [`verta.yaml`](verta.yaml) | UART 직접 제어, 9600 / 8N1, RX GPIO1 / TX GPIO2; 보드·네트워크 설정 별도 |
| [`verta_gl_s10.yaml`](verta_gl_s10.yaml) | GL-S10 Ethernet + 루버 AP Wi-Fi 연결; 유선 base와 `friendly_name` 필요 |
| [`verta_ap.yaml`](verta_ap.yaml) | UART→HTTP 중계 예제, RX GPIO26 / TX GPIO32; 보드 설정 별도 |

## 직접 UART 구성

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base.yaml
      - packages/louver/verta/verta.yaml
```

[공통 설정](../../README.md#common-device-setup)의 이름·Wi-Fi/OTA secrets와
사용 보드의 `esp32:` 설정을 추가하세요. UART 핀은 보드에 맞게 변경하세요.

## GL-S10 구성

```yaml
packages:
  remote:
    url: https://github.com/eigger/espcomponents
    ref: master
    files:
      - packages/device_base_wired.yaml
      - packages/louver/verta/verta_gl_s10.yaml
```

`name`, `friendly_name`, `ota_password`와 함께 아래 secrets가 필요합니다:

```yaml
verta_wifi_ssid: "YOUR_LOUVER_AP"
verta_wifi_password: "YOUR_LOUVER_AP_PASSWORD"
```

이 구성은 Ethernet과 루버 AP용 Wi-Fi를 함께 사용합니다. HTTP 대상은 YAML에
`172.217.28.1`로 지정되어 있으므로 설치 환경과 일치하는지 확인하세요.

`verta_ap.yaml`은 `louver_` SSID와 `github://pr#7884`의 `http_request`를
사용하는 별도 예제입니다. 이 의존성과 HTTP 엔드포인트를 검토한 뒤 적용하세요.
세 프리셋은 서로 대체 관계이며 한 장치에 모두 포함하지 않습니다.

[패키지 목록](../../README.md)
