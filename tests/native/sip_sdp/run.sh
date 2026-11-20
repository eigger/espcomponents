#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
INC=(-I"$ROOT/components/sip_client")
CFLAGS=(-std=c99 -Wall -Wextra -Werror)
CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror)
OUT="${TMPDIR:-/tmp}"

g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/components/sip_client/sip_message.cpp" \
  "$ROOT/tests/native/sip_sdp/test_parse_sdp.cpp" \
  -o "$OUT/sip_sdp_parse_test"
"$OUT/sip_sdp_parse_test"

g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/components/sip_client/sip_message.cpp" \
  "$ROOT/tests/native/sip_sdp/test_sip_message.cpp" \
  -o "$OUT/sip_message_test"
"$OUT/sip_message_test"

g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/components/sip_client/sdp_builder.cpp" \
  "$ROOT/tests/native/sip_sdp/test_sdp_builder.cpp" \
  -o "$OUT/sip_sdp_builder_test"
"$OUT/sip_sdp_builder_test"

g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/components/sip_client/dtmf.cpp" \
  "$ROOT/tests/native/sip_sdp/test_dtmf.cpp" \
  -o "$OUT/sip_dtmf_test"
"$OUT/sip_dtmf_test"

g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/tests/native/sip_sdp/test_g711_codec.cpp" \
  -o "$OUT/sip_g711_codec_test"
"$OUT/sip_g711_codec_test"

# Compile libg722 as C (not C++): private headers are gated on G722_INTERNAL
# and must see the real struct layouts.
gcc "${CFLAGS[@]}" "${INC[@]}" -c \
  "$ROOT/components/sip_client/g722_encode.c" \
  -o "$OUT/g722_encode.o"
gcc "${CFLAGS[@]}" "${INC[@]}" -c \
  "$ROOT/components/sip_client/g722_decode.c" \
  -o "$OUT/g722_decode.o"
g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$OUT/g722_encode.o" "$OUT/g722_decode.o" \
  "$ROOT/tests/native/sip_sdp/test_g722_codec.cpp" \
  -o "$OUT/sip_g722_codec_test"
"$OUT/sip_g722_codec_test"

# Silence frames across every codec (what RtpSession sends to keep the stream up).
g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$OUT/g722_encode.o" "$OUT/g722_decode.o" \
  "$ROOT/tests/native/sip_sdp/test_codec_silence.cpp" \
  -o "$OUT/sip_codec_silence_test"
"$OUT/sip_codec_silence_test"

# Transport-derived Via/Contact strings (UDP must stay byte-identical).
g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/tests/native/sip_sdp/test_sip_transport.cpp" \
  -o "$OUT/sip_transport_test"
"$OUT/sip_transport_test"

# WebSocket transport pure helpers: .invalid host, URI, frame reassembly.
g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/tests/native/sip_sdp/test_sip_ws.cpp" \
  -o "$OUT/sip_ws_test"
"$OUT/sip_ws_test"

# RTP idle keepalive scheduler (thinned silence for NAT / symmetric-RTP latching).
g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/tests/native/sip_sdp/test_rtp_keepalive.cpp" \
  -o "$OUT/sip_rtp_keepalive_test"
"$OUT/sip_rtp_keepalive_test"

# In-dialog logic: tag extraction, dialog matching, SDP direction answers,
# re-offer diffing, RFC 4028 session timers.
g++ "${CXXFLAGS[@]}" "${INC[@]}" \
  "$ROOT/components/sip_client/sip_message.cpp" \
  "$ROOT/components/sip_client/sip_dialog.cpp" \
  "$ROOT/tests/native/sip_sdp/test_sip_dialog.cpp" \
  -o "$OUT/sip_dialog_test"
"$OUT/sip_dialog_test"
