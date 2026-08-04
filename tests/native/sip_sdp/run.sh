#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
INC=(-I"$ROOT/components/sip_client")
CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror)
OUT="${TMPDIR:-/tmp}"

g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/components/sip_client/sip_message.cpp" \
  "$ROOT/tests/native/sip_sdp/test_parse_sdp.cpp" \
  -o "$OUT/sip_sdp_parse_test"
"$OUT/sip_sdp_parse_test"

g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/components/sip_client/sdp_builder.cpp" \
  "$ROOT/tests/native/sip_sdp/test_sdp_builder.cpp" \
  -o "$OUT/sip_sdp_builder_test"
"$OUT/sip_sdp_builder_test"

g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/tests/native/sip_sdp/test_g711_codec.cpp" \
  -o "$OUT/sip_g711_codec_test"
"$OUT/sip_g711_codec_test"
