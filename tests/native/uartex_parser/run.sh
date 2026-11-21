#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
INC=(-I"$ROOT/components/uartex")
SRC=("$ROOT/components/uartex/parser.cpp" "$ROOT/components/uartex/checksum.cpp")
CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror)

g++ "${CXXFLAGS[@]}" "${INC[@]}" "${SRC[@]}" \
  "$ROOT/tests/native/uartex_parser/test_parser.cpp" \
  -o "${TMPDIR:-/tmp}/uartex_parser_test"
"${TMPDIR:-/tmp}/uartex_parser_test"

g++ "${CXXFLAGS[@]}" "${INC[@]}" "${SRC[@]}" \
  "$ROOT/tests/native/uartex_parser/test_checksum.cpp" \
  -o "${TMPDIR:-/tmp}/uartex_checksum_test"
"${TMPDIR:-/tmp}/uartex_checksum_test"

# Exercise the actual component receive path using minimal ESPHome host stubs.
g++ -std=c++17 -I"$ROOT/tests/native/uartex_parser/stubs" "${INC[@]}" \
  "$ROOT/components/uartex/uartex.cpp" "$ROOT/components/uartex/uartex_device.cpp" "${SRC[@]}" \
  "$ROOT/tests/native/uartex_parser/test_component.cpp" \
  -o "${TMPDIR:-/tmp}/uartex_component_test"
"${TMPDIR:-/tmp}/uartex_component_test"
