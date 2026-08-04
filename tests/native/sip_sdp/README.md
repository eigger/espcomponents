# sip_client SDP native tests

Host-side tests for `parse_sdp()` in `components/sip_client/sip_message.cpp`
(no ESPHome runtime dependency).

## Run

```bash
tests/native/sip_sdp/run.sh
```

## Coverage

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

CI: `.github/workflows/sip-sdp-tests.yml`
