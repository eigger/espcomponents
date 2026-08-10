# BLE ELM327 native parser tests

`response_parser.cpp`(`parse_response_bytes` / `normalize_command`)를 호스트에서 검증합니다.
ESPHome/ESP-IDF 프레임워크에 의존하지 않는 순수 파싱 로직만 분리되어 있어, 실제
BLE 스택 없이도 응답 파싱 회귀를 잡을 수 있습니다.

## 실행

```bash
tests/native/ble_elm327_parser/run.sh
```

Windows (g++ 필요):

```powershell
$root = (Resolve-Path "$PSScriptRoot\..\..\..").Path
g++ -std=c++17 -Wall -Wextra -Werror `
  -I"$root/components/ble_elm327" `
  "$root/components/ble_elm327/response_parser.cpp" `
  "$root/tests/native/ble_elm327_parser/test_parser.cpp" `
  -o ble_elm327_parser_test.exe
.\ble_elm327_parser_test.exe
```

## 커버 시나리오

| 테스트 | 조건 |
|--------|------|
| `multiline_hex_index_regression_uppercase` | #294 회귀: `9:` 다음 `A:` 라인이 잘리지 않고 이어붙는지 |
| `multiline_hex_index_regression_full_range` | 0~9, A~F 16개 라인 전체가 잘림 없이 조립되는지 (실제 108바이트 응답 재현 시나리오) |
| `multiline_hex_index_lowercase` | 소문자 인덱스(`a:`, `b:`)도 허용되는지 |
| `multiline_numeric_indices_only` | 순수 숫자 인덱스만 있는 짧은 응답 |
| `singleline_response_no_index` | 콜론 인덱스가 없는 단일 라인 응답 |
| `command_echo_is_skipped` | 에코된 명령 라인이 페이로드에서 제외되는지 |
| `non_hex_prefixed_colon_line_is_not_multiline` | `OK:` 처럼 hex가 아닌 접두사는 멀티라인으로 오인되지 않는지 |
| `short_response_is_ignored` | 4자리(2바이트) 미만 hex는 무시되는지 |
| `odd_length_hex_drops_trailing_nibble` | 홀수 길이 hex의 마지막 니블이 버려지는지 |
| `blank_lines_and_whitespace_are_ignored` | 빈 줄/공백 줄이 조립을 깨지 않는지 |
| `empty_response_yields_no_bytes` | 빈 응답 처리 |
| `normalize_command_*` | 명령어 정규화(공백 제거, 소문자화) |

CI: `.github/workflows/ble-elm327-parser-tests.yml`
(`components/ble_elm327`, `tests/native/ble_elm327_parser` 변경 시에만 트리거되는 별도
워크플로 — 관련 없는 PR에는 체크 자체가 나타나지 않음).
