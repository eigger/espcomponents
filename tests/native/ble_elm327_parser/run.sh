#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
INC=(-I"$ROOT/components/ble_elm327")
SRC=("$ROOT/components/ble_elm327/response_parser.cpp")
CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror)

g++ "${CXXFLAGS[@]}" "${INC[@]}" "${SRC[@]}" \
  "$ROOT/tests/native/ble_elm327_parser/test_parser.cpp" \
  -o "${TMPDIR:-/tmp}/ble_elm327_parser_test"
"${TMPDIR:-/tmp}/ble_elm327_parser_test"
