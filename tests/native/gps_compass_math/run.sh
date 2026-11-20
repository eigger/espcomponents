#!/usr/bin/env bash
# Host tests for components/gps_compass/compass_math.h (header-only, no ESPHome needed).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
INC=(-I"$ROOT/components/gps_compass")
CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror)
OUT="${TMPDIR:-/tmp}"

g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/tests/native/gps_compass_math/test_compass_math.cpp" \
  -o "$OUT/gps_compass_math_test"
"$OUT/gps_compass_math_test"
