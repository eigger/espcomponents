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
| `dynamic_pt_before_g722_no_false_positive` | `m=audio … 96 9 0 8 101` must not treat 96–99 as PT 9; `g722_pt == 9` |
| `static_g722_without_rtpmap` | RFC 3551 static PT 9 without rtpmap |
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
| `offer_g722_preferred` | offer with `[g722, pcmu]` → `9 0 101` |
| `answer_single_codec_dynamic_pcma` | answer PT 97 + `PCMA/8000` (not PCMU) |
| `answer_without_dtmf` | no telephone-event when dtmf_pt < 0 |

### DTMF receive (`test_dtmf.cpp`)

| Test | Intent |
|------|--------|
| `digit_fires_once_per_event` | all packets of one RFC 4733 event (incl. the 3 end retransmits) → 1 digit |
| `marker_less_sender_still_fires` | Yealink DECT / 3CX Android send no marker bit |
| `same_digit_pressed_twice` | new RTP timestamp = new press |
| `star_hash_and_letters` | events 10/11/12/15 → `*` `#` `A` `D`; 16 (flash) dropped |
| `reset_between_calls` | `stop()`/`start()` clears dedup state |
| `info_dtmf_relay` | `Signal=1`, case/space tolerance, Content-Type params |
| `info_star_hash_spellings` | `Signal=*` and `Signal=10`/`11` both accepted |
| `info_plain_dtmf` | `application/dtmf` bare-digit body |
| `info_duration_is_not_a_digit` | `Duration=250` must not be read as DTMF `D` |
| `info_non_dtmf_is_ignored` | other INFO content types / out-of-range codes → no digit |
| `info_alternate_keys_and_types` | `d=` / `dtmf=` keys, `audio/telephone-event`, missing Content-Type |
| `unnegotiated_telephone_event_pt` | 4-byte payload on a dynamic PT is DTMF; audio PT and static PTs are not |

### `G711Codec` (`test_g711_codec.cpp`)

| Test | Intent |
|------|--------|
| framing constants | pcm/payload/ts split for µ-law and dynamic A-law |
| roundtrip SNR | encode→decode tone ≥ 25 dB |
| dynamic PT A-law | PT 97 uses A-law bytes, not µ-law |

### `G722Codec` (`test_g722_codec.cpp`)

| Test | Intent |
|------|--------|
| framing constants | G722/8000 rtpmap, 16 kHz PCM, 320/160 framing |
| roundtrip SNR | encode→decode 1 kHz tone ≥ 20 dB (libg722) |

CI: `.github/workflows/sip-sdp-tests.yml`
