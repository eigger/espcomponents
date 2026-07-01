#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT="${TMPDIR:-/tmp}/uartex_parser_test"

g++ -std=c++17 -Wall -Wextra -Werror \
  -I"$ROOT/components/uartex" \
  "$ROOT/components/uartex/parser.cpp" \
  "$ROOT/tests/native/uartex_parser/test_parser.cpp" \
  -o "$OUT"

"$OUT"
