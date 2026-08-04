# sip_client SDP / codec native tests

Host-side tests with no ESPHome runtime dependency.

## Run

```bash
tests/native/sip_sdp/run.sh
```

## Coverage

### `parse_sdp` (`test_parse_sdp.cpp`)

| Test | Intent |
|------|--------|
| `dynamic_pt_before_g722_no_false_positive` | `m=audio … 96 9 0 8 101` must not treat 96–99 as PT 9 |
| `static_only_without_rtpmap` | RFC 3551 static 0/8 without rtpmap |
| `no_telephone_event` | DTMF absent → `telephone_event_pt == -1` |
| `no_common_g711` | Opus-only offer → no pcmu/pcma convenience PT |
| `pt96_through_99_not_confused_with_9` | Substring regression for `" 9"` |
| `audio_plus_video_ignores_video_section` | video `c=` / `rtpmap` must not pollute audio |
| `non_numeric_fmt_token_skipped` | `*` etc. must not become PT 0 via `atoi` |
| `rejected_audio_stream_port_zero` | `m=audio 0` still parses |
| `port_with_number_of_ports_suffix` | `m=audio 12345/2` → port 12345 |

### `build_sdp_body` (`test_sdp_builder.cpp`)

| Test | Intent |
|------|--------|
| `offer_lists_both_g711_and_dtmf` | offer m-line `0 8 101` |
| `answer_single_codec_dynamic_pcma` | answer PT 97 + `PCMA/8000` (not PCMU) |
| `answer_without_dtmf` | no telephone-event when dtmf_pt < 0 |

### `G711Codec` (`test_g711_codec.cpp`)

| Test | Intent |
|------|--------|
| framing constants | pcm/payload/ts split for µ-law and dynamic A-law |
| roundtrip SNR | encode→decode tone ≥ 25 dB |
| dynamic PT A-law | PT 97 uses A-law bytes, not µ-law |

CI: `.github/workflows/sip-sdp-tests.yml`
