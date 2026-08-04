#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
INC=(-I"$ROOT/components/sip_client")
SRC=("$ROOT/components/sip_client/sip_message.cpp")
CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror)

g++ "${CXXFLAGS[@]}" "${INC[@]}" "${SRC[@]}" \
  "$ROOT/tests/native/sip_sdp/test_parse_sdp.cpp" \
  -o "${TMPDIR:-/tmp}/sip_sdp_test"
"${TMPDIR:-/tmp}/sip_sdp_test"
