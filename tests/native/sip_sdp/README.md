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
| `direction_default_sendrecv` | no `a=` direction → `sendrecv`, not on hold |
| `direction_session_level` | session-level `a=sendonly` applies to the audio stream |
| `direction_media_level_overrides_session` | media-level direction beats the session-level one (both ways) |
| `direction_all_values_and_case` | `sendrecv` / `sendonly` / `recvonly` / `inactive`; value case-insensitive |
| `video_direction_does_not_leak_into_audio` | a video section's `a=inactive` / `a=sendonly` must not change audio's direction |
| `connection_zero_is_hold` | `c=IN IP4 0.0.0.0` (session or audio level) and `::` = legacy hold; media-level address overrides; video `c=` ignored |

### SIP header parsing / routing helpers (`test_sip_message.cpp`)

| Test | Intent |
|------|--------|
| `multiple_via_kept_in_order` | INVITE through a proxy/SBC has one Via per hop; all kept, topmost first; `via_branch` reads the top one |
| `record_route_preserved` | Record-Route survives parsing and `;lr` is detected |
| `repeated_record_route_rows_and_comma_list_are_equivalent` | repeated rows vs one comma list → same ordered route set |
| `non_list_header_keeps_first_value` | Contact etc. still keep the first occurrence |
| `compact_via_is_merged` | `v:` and `Via:` rows merge into one chain |
| `header_folding` | leading-whitespace continuation lines are joined |
| `split_ignores_nested_commas` | commas inside `"..."` / `<...>` are not separators |
| `via_branch_variants` | param order, case, comma-joined chain, missing branch |
| `loose_route_detection` | `;lr`, `;lr=`, `;lr;`, case; `;lrx` and user-part `lr` are strict |
| `extract_angle_uri` | `<...>` vs bare `sip:` vs garbage |
| `route_set_empty_keeps_target` | no / blank route set → Request-URI untouched, no Route |
| `route_set_loose_router` | `;lr` first hop: Request-URI = remote target, one `Route:` per hop |
| `route_set_strict_router` | strict first hop takes the Request-URI, remote target appended |
| `route_set_from_reversed_2xx_record_route` | UAC route set = 2xx Record-Route reversed |

### `build_sdp_body` (`test_sdp_builder.cpp`)

| Test | Intent |
|------|--------|
| `offer_lists_both_g711_and_dtmf` | offer m-line `0 8 101` |
| `offer_g722_preferred` | offer with `[g722, pcmu]` → `9 0 101` |
| `answer_single_codec_dynamic_pcma` | answer PT 97 + `PCMA/8000` (not PCMU) |
| `answer_without_dtmf` | no telephone-event when dtmf_pt < 0 |
| `origin_version_defaults_to_session_id` | no explicit version → `o=- <id> <id>` as before |
| `origin_version_explicit` | session id stays, version differs and is deterministic (RFC 3264 §8) |
| `negotiated_direction_in_body` | `a=` carries the given direction incl. `inactive`; hold answer keeps the m= line |

### In-dialog logic (`test_sip_dialog.cpp`)

| Test | Intent |
|------|--------|
| `header_tag_variants` | tag from name-addr / addr-spec, case-insensitive name, spaces around `=` |
| `header_tag_ignores_lookalikes` | `tag=` inside a quoted display name or URI parameters is not the header tag |
| `parse_cseq` | `"6 INVITE"` → number + method; malformed values rejected |
| `dialog_match` | Call-ID + To tag (= ours) + From tag (= peer's) must match; no To tag = new dialog; wrong tag/Call-ID → 481 verdict |
| `dialog_cseq_rules` | lower / equal CSeq → 500 verdict; same CSeq + method as the stored response → retransmission; next CSeq OK; outbound starts at 0 |
| `direction_names` | name ↔ enum ↔ send/receive flags |
| `answer_direction_full_duplex` | RFC 3264 §6.1: sendonly→recvonly, recvonly→sendonly, sendrecv→sendrecv, inactive→inactive |
| `answer_direction_capability` | speaker-only / mic-only / no endpoint intersect the answer |
| `answer_direction_legacy_hold` | `c=0.0.0.0` means we must not send |
| `diff_media` | codec / PT change and IPv4↔IPv6 need a restart; port / address / DTMF PT change are live |
| `usable_audio` | port 0 or empty body is not a usable re-offer |
| `parse_session_expires` | delta, `refresher=uac` or `uas`, whitespace / case, garbage |
| `compact_session_expires_header` | `x:` → Session-Expires, `k:` → Supported, Min-SE |
| `parse_min_se_and_lists` | Min-SE value; `timer` in Supported / Require lists (case, params, prefix) |
| `decide_*` | RFC 4028 decision: no header → none; < 90 s → 422 + Min-SE 90; named `uac` kept; unnamed + UAC supports timer → `uac`; UAC without support or `refresher=uas` → no Session-Expires (we never refresh); `Require: timer` only when the UAC supports it |
| `decide_refresh_without_header_ends_timer` | a refresh without Session-Expires (or with `refresher=uas`) ends an agreed timer; no echo of the old interval (RFC 4028 §9) |
| `ack_matches` | ACK must carry the dialog's Call-ID and the expected CSeq (ACK of another call's 486 is ignored) |
| `offer_collides` | 491 only for a new offer (re-INVITE, UPDATE with SDP) while our offer-less 200 is unanswered |
| `early_outbound_dialog_match` | UPDATE in the early dialog of our own INVITE matches on Call-ID + our tag |
| `codec_choice_keeps_current` | a re-offer that still has the codec/PT in use keeps it; otherwise preference order; unconfigured/no-common handled |
| `peer_refreshes_in_response` | 2xx `refresher=uas` means the peer refreshes |
| `session_watchdog_ms` | expiry − min(32 s, interval/3); overflow-safe |

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
