# 에어컨 패키지

삼성 시스템 에어컨의 RS-485 통신 구성입니다. 아래 두 구현을 구분해서 선택하세요.

| 파일 | 구현 / 상태 |
| --- | --- |
| [`samsung/samsung_ac.yaml`](samsung/samsung_ac.yaml) | 별도 `samsung_ac` 외부 컴포넌트 기반 NASA 구성 |
| [`samsung/samsung_system.yaml`](samsung/samsung_system.yaml) | `uartex` 기반 작성 중인 구성 |

[삼성 설치·배선 안내](samsung/README.md)에서 AtomS3 Lite + Tail485의 핀과
F1/F2 결선을 확인하세요. `samsung_ac.yaml`에는 현장별 실내기 주소와 방 구성이
들어 있으므로 그대로 포함하기 전에 자신의 설치 주소에 맞춰야 합니다.

장치 이름·Wi-Fi·API·OTA는 [공통 장치 설정](../README.md#common-device-setup)을
사용할 수 있습니다. 선택한 에어컨 패키지와 보드 설정이 일치하는지도 확인하세요.

[패키지 목록](../README.md)
