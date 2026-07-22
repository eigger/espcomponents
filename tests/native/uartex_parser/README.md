# UARTEx native tests

`Parser` 및 내장 `compute_checksum` (ADD/XOR/…) 을 호스트에서 검증합니다.

## 실행

```bash
tests/native/uartex_parser/run.sh
```

Windows (g++ 필요):

```powershell
$root = (Resolve-Path "$PSScriptRoot\..\..\..").Path
g++ -std=c++17 -Wall -Wextra -Werror `
  -I"$root/components/uartex" `
  "$root/components/uartex/parser.cpp" `
  "$root/tests/native/uartex_parser/test_parser.cpp" `
  -o uartex_parser_test.exe
.\uartex_parser_test.exe
```

## 커버 시나리오

### Parser (`test_parser.cpp`)

| 테스트 | 조건 |
|--------|------|
| `no_header_fixed_length` | 헤더 없음, `total_len`만 |
| `single_header_fixed_length` | 단일 헤더 + 고정 길이 |
| `single_header_with_checksum` | 체크섬 1바이트, 파서 자동 완료 없음 |
| `multi_header_candidates` | 헤더 후보 2개 (0xA0 / 0xA1) |
| `header_mismatch_clears_buffer` | 헤더 불일치 시 버퍼 클리어 |
| `header_with_mask` | 헤더 마스크 매칭 |
| `header_footer_checksum` | 헤더 + 푸터 + 체크섬 |
| `fixed_length_with_footer` | 고정 길이 + 푸터 |
| `dynamic_data_length_*` | 가변 길이 필드 (BE / LE) |
| `no_footer_no_length_incomplete` | 푸터·길이 없으면 미완료 |
| `apply_mask` / `clear` / `buffer_len` | 유틸·상태 리셋 |

### Checksum (`test_checksum.cpp`)

| 테스트 | 조건 |
|--------|------|
| `NONE` / `CUSTOM` | 0 반환 |
| `ADD` / `XOR` | 헤더 포함 |
| `ADD_NO_HEADER` / `XOR_NO_HEADER` | payload만 |
| `XOR_ADD` | 2바이트 checksum |
| Parser 연동 | `compute_checksum`으로 만든 패킷이 `verify_checksum` 통과 |

CI: `.github/workflows/uartex-parser-tests.yml` (`components/uartex`, `tests/components/uartex`, `tests/native/uartex_parser` 변경 시에만 트리거되는 별도 워크플로 — 관련 없는 PR에는 체크 자체가 나타나지 않음).
