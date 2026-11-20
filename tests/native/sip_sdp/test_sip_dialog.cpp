#include "sip_dialog.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace esphome::sip_client;

namespace {

int g_failures = 0;

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    g_failures++;
  }
}

void require_eq(long long actual, long long expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got " << actual << ", expected " << expected << ")\n";
    g_failures++;
  }
}

void require_eq_str(const std::string &actual, const std::string &expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got \"" << actual << "\", expected \"" << expected
              << "\")\n";
    g_failures++;
  }
}

// ---------------- tags ----------------

void test_header_tag_variants() {
  require_eq_str(header_tag("<sip:100@pbx>;tag=abc123"), "abc123", "name-addr tag");
  require_eq_str(header_tag("\"Bob\" <sip:bob@pbx>;tag=xyz;foo=bar"), "xyz", "tag before other params");
  require_eq_str(header_tag("<sip:bob@pbx>;foo=bar;TAG=Up"), "Up", "param name is case-insensitive");
  require_eq_str(header_tag("sip:bob@pbx;tag=t1"), "t1", "bare addr-spec");
  require_eq_str(header_tag("<sip:bob@pbx>"), "", "no tag");
  require_eq_str(header_tag("sip:bob@pbx"), "", "bare, no tag");
  require_eq_str(header_tag("<sip:bob@pbx>;tag = spaced"), "spaced", "whitespace around =");
  require_eq_str(header_tag(""), "", "empty");
}

void test_header_tag_ignores_lookalikes() {
  // tag= inside the quoted display name or inside the URI params is not the header tag.
  require_eq_str(header_tag("\"x;tag=evil\" <sip:bob@pbx>;tag=real"), "real", "tag= in display name");
  require_eq_str(header_tag("\"x;tag=evil\" <sip:bob@pbx>"), "", "only a display-name tag= -> none");
  require_eq_str(header_tag("<sip:bob@pbx;tag=uri>;tag=hdr"), "hdr", "tag= in URI params");
  require_eq_str(header_tag("<sip:bob@pbx;tag=uri>"), "", "only a URI tag= -> none");
  require_eq_str(header_tag("\"a<b\" <sip:bob@pbx>;tag=q"), "q", "'<' inside quoted name");
  require_eq_str(header_tag("<sip:bob@pbx;tag=1>;x=y;tag=2"), "2", "first header tag after >");
  require_eq_str(header_tag("<sip:bob@pbx"), "", "unterminated bracket");
}

// ---------------- CSeq ----------------

void test_parse_cseq() {
  uint32_t n = 0;
  std::string m;
  require(parse_cseq("314159 INVITE", n, m), "plain cseq");
  require_eq(n, 314159, "cseq number");
  require_eq_str(m, "INVITE", "cseq method");
  require(parse_cseq("  7   UPDATE ", n, m), "padded cseq");
  require_eq(n, 7, "padded number");
  require_eq_str(m, "UPDATE", "padded method");
  require(!parse_cseq("INVITE", n, m), "missing number");
  require(!parse_cseq("12", n, m), "missing method");
  require(!parse_cseq("x1 INVITE", n, m), "non-numeric");
  require(!parse_cseq("", n, m), "empty");
}

// ---------------- dialog classification ----------------

SipMessage req(const std::string &method, const std::string &from, const std::string &to,
               const std::string &call_id, const std::string &cseq) {
  std::string raw = method + " sip:esp@10.0.0.2 SIP/2.0\r\n";
  raw += "Via: SIP/2.0/UDP 10.0.0.1;branch=z9hG4bKabc\r\n";
  raw += "From: " + from + "\r\n";
  raw += "To: " + to + "\r\n";
  raw += "Call-ID: " + call_id + "\r\n";
  raw += "CSeq: " + cseq + "\r\n";
  raw += "Content-Length: 0\r\n\r\n";
  return parse_sip_message(raw);
}

DialogState dialog() {
  DialogState st;
  st.call_id = "call-1@pbx";
  st.local_tag = "ours";
  st.remote_tag = "theirs";
  st.remote_cseq = 5;
  return st;
}

void test_dialog_match() {
  const DialogState st = dialog();
  auto ok = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "call-1@pbx", "6 INVITE");
  require(classify_dialog_request(st, ok) == DialogVerdict::OK, "matching re-INVITE is OK");

  auto no_to_tag = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>", "call-1@pbx", "6 INVITE");
  require(classify_dialog_request(st, no_to_tag) == DialogVerdict::NEW_DIALOG, "no To tag = new dialog");

  auto wrong_call = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "other@pbx", "6 INVITE");
  require(classify_dialog_request(st, wrong_call) == DialogVerdict::NOT_IN_DIALOG, "wrong Call-ID");
  auto wrong_to = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=nope", "call-1@pbx", "6 INVITE");
  require(classify_dialog_request(st, wrong_to) == DialogVerdict::NOT_IN_DIALOG, "wrong To tag");
  auto wrong_from = req("INVITE", "<sip:pbx>;tag=zzz", "<sip:esp>;tag=ours", "call-1@pbx", "6 INVITE");
  require(classify_dialog_request(st, wrong_from) == DialogVerdict::NOT_IN_DIALOG, "wrong From tag");
  auto no_from = req("INVITE", "<sip:pbx>", "<sip:esp>;tag=ours", "call-1@pbx", "6 INVITE");
  require(classify_dialog_request(st, no_from) == DialogVerdict::NOT_IN_DIALOG, "missing From tag");

  DialogState empty;
  require(classify_dialog_request(empty, ok) == DialogVerdict::NOT_IN_DIALOG, "no dialog at all");

  // A tag= lookalike in the display name must not satisfy the match.
  auto spoof = req("INVITE", "\"x;tag=theirs\" <sip:pbx>", "<sip:esp>;tag=ours", "call-1@pbx", "6 INVITE");
  require(classify_dialog_request(st, spoof) == DialogVerdict::NOT_IN_DIALOG, "display-name tag spoof");
}

void test_dialog_cseq_rules() {
  DialogState st = dialog();
  auto lower_cseq = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "call-1@pbx", "4 INVITE");
  require(classify_dialog_request(st, lower_cseq) == DialogVerdict::STALE_CSEQ, "lower CSeq -> 500");
  auto equal_cseq = req("UPDATE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "call-1@pbx", "5 UPDATE");
  require(classify_dialog_request(st, equal_cseq) == DialogVerdict::STALE_CSEQ, "equal CSeq, nothing stored");
  auto bad = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "call-1@pbx", "garbage");
  require(classify_dialog_request(st, bad) == DialogVerdict::STALE_CSEQ, "unparsable CSeq");

  // After answering CSeq 6 INVITE, the same request again is a retransmission.
  st.remote_cseq = 6;
  st.last_response_cseq = 6;
  st.last_response_method = "INVITE";
  auto retrans = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "call-1@pbx", "6 INVITE");
  require(classify_dialog_request(st, retrans) == DialogVerdict::RETRANSMISSION, "same CSeq+method");
  auto other_method = req("UPDATE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "call-1@pbx", "6 UPDATE");
  require(classify_dialog_request(st, other_method) == DialogVerdict::STALE_CSEQ,
          "same CSeq, other method is not a retransmission");
  auto next = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "call-1@pbx", "7 INVITE");
  require(classify_dialog_request(st, next) == DialogVerdict::OK, "next CSeq");
  auto older = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "call-1@pbx", "5 INVITE");
  require(classify_dialog_request(st, older) == DialogVerdict::STALE_CSEQ, "older than stored");

  // Outbound call: remote CSeq starts at 0, the PBX's first request is 1+.
  DialogState out = dialog();
  out.remote_cseq = 0;
  auto first = req("INVITE", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", "call-1@pbx", "1 INVITE");
  require(classify_dialog_request(out, first) == DialogVerdict::OK, "outbound first remote request");
}

// ---------------- direction ----------------

void test_direction_names() {
  require_eq_str(direction_name(SdpDirection::SENDRECV), "sendrecv", "name sendrecv");
  require_eq_str(direction_name(SdpDirection::SENDONLY), "sendonly", "name sendonly");
  require_eq_str(direction_name(SdpDirection::RECVONLY), "recvonly", "name recvonly");
  require_eq_str(direction_name(SdpDirection::INACTIVE), "inactive", "name inactive");
  require(direction_from_name("recvonly") == SdpDirection::RECVONLY, "parse recvonly");
  require(direction_from_name("INACTIVE") == SdpDirection::INACTIVE, "parse case-insensitive");
  require(direction_from_name("bogus") == SdpDirection::SENDRECV, "unknown -> sendrecv");
  require(direction_from_flags(true, true) == SdpDirection::SENDRECV, "flags sr");
  require(direction_from_flags(true, false) == SdpDirection::SENDONLY, "flags so");
  require(direction_from_flags(false, true) == SdpDirection::RECVONLY, "flags ro");
  require(direction_from_flags(false, false) == SdpDirection::INACTIVE, "flags in");
}

void test_answer_direction_full_duplex() {
  const SdpDirection cap = SdpDirection::SENDRECV;
  require(answer_direction(SdpDirection::SENDRECV, false, cap) == SdpDirection::SENDRECV, "sr -> sr");
  require(answer_direction(SdpDirection::SENDONLY, false, cap) == SdpDirection::RECVONLY, "so -> ro");
  require(answer_direction(SdpDirection::RECVONLY, false, cap) == SdpDirection::SENDONLY, "ro -> so");
  require(answer_direction(SdpDirection::INACTIVE, false, cap) == SdpDirection::INACTIVE, "in -> in");
}

void test_answer_direction_capability() {
  // Speaker only (no mic): never send.
  const SdpDirection ro = SdpDirection::RECVONLY;
  require(answer_direction(SdpDirection::SENDRECV, false, ro) == SdpDirection::RECVONLY, "sr vs ro cap");
  require(answer_direction(SdpDirection::SENDONLY, false, ro) == SdpDirection::RECVONLY, "so vs ro cap");
  require(answer_direction(SdpDirection::RECVONLY, false, ro) == SdpDirection::INACTIVE, "ro vs ro cap");
  // Mic only: never receive.
  const SdpDirection so = SdpDirection::SENDONLY;
  require(answer_direction(SdpDirection::SENDRECV, false, so) == SdpDirection::SENDONLY, "sr vs so cap");
  require(answer_direction(SdpDirection::RECVONLY, false, so) == SdpDirection::SENDONLY, "ro vs so cap");
  require(answer_direction(SdpDirection::SENDONLY, false, so) == SdpDirection::INACTIVE, "so vs so cap");
  require(answer_direction(SdpDirection::SENDRECV, false, SdpDirection::INACTIVE) == SdpDirection::INACTIVE,
          "no audio endpoint at all");
}

void test_answer_direction_legacy_hold() {
  // c=0.0.0.0 with sendrecv: the peer takes no media, so we must not send.
  const SdpDirection cap = SdpDirection::SENDRECV;
  require(answer_direction(SdpDirection::SENDRECV, true, cap) == SdpDirection::RECVONLY,
          "legacy hold: we stop sending");
  require(answer_direction(SdpDirection::SENDONLY, true, cap) == SdpDirection::RECVONLY,
          "sendonly + 0.0.0.0");
  require(answer_direction(SdpDirection::INACTIVE, true, cap) == SdpDirection::INACTIVE,
          "inactive + 0.0.0.0");
}

// ---------------- media diff ----------------

NegotiatedMedia media(int pt, int id, const char *ip, uint16_t port, int dtmf) {
  NegotiatedMedia m;
  m.pt = pt;
  m.codec_id = id;
  m.ip = ip;
  m.port = port;
  m.dtmf_pt = dtmf;
  return m;
}

void test_diff_media() {
  const NegotiatedMedia cur = media(8, 1, "10.0.0.1", 4000, 101);
  MediaDelta none = diff_media(cur, cur);
  require(!none.any(), "identical -> no change");
  require(!none.restart_needed(), "identical -> no restart");

  MediaDelta port = diff_media(cur, media(8, 1, "10.0.0.1", 4002, 101));
  require(port.remote_changed && !port.restart_needed(), "port change: set_remote only");
  MediaDelta ip = diff_media(cur, media(8, 1, "10.0.0.9", 4000, 101));
  require(ip.remote_changed && !ip.restart_needed(), "ipv4 change: set_remote only");

  MediaDelta codec = diff_media(cur, media(0, 0, "10.0.0.1", 4000, 101));
  require(codec.codec_changed && codec.restart_needed(), "codec change restarts");
  MediaDelta pt_only = diff_media(cur, media(97, 1, "10.0.0.1", 4000, 101));
  require(pt_only.codec_changed && pt_only.restart_needed(), "PT change (same codec) restarts");

  MediaDelta dtmf = diff_media(cur, media(8, 1, "10.0.0.1", 4000, 96));
  require(dtmf.dtmf_changed && !dtmf.restart_needed(), "dtmf PT change is live");

  MediaDelta v6 = diff_media(cur, media(8, 1, "fd00::1", 4000, 101));
  require(v6.family_changed && v6.restart_needed(), "IPv4 -> IPv6 restarts");
  MediaDelta v6b = diff_media(media(8, 1, "fd00::1", 4000, 101), media(8, 1, "fd00::2", 4000, 101));
  require(!v6b.family_changed && !v6b.restart_needed(), "IPv6 -> IPv6 does not");
  require(ip_is_ipv6("::1") && !ip_is_ipv6("1.2.3.4"), "ip_is_ipv6");
}

void test_usable_audio() {
  SdpInfo sdp = parse_sdp("c=IN IP4 1.2.3.4\r\nm=audio 4000 RTP/AVP 0\r\n");
  require(sdp_has_usable_audio(sdp), "normal audio is usable");
  SdpInfo rejected = parse_sdp("c=IN IP4 1.2.3.4\r\nm=audio 0 RTP/AVP 0\r\n");
  require(!sdp_has_usable_audio(rejected), "port 0 rejected");
  require(!sdp_has_usable_audio(parse_sdp("")), "empty body");
}

// ---------------- session timers: parsing ----------------

void test_parse_session_expires() {
  SessionExpires se = parse_session_expires("1800;refresher=uac");
  require(se.valid && se.delta == 1800 && se.refresher == Refresher::UAC, "uac");
  se = parse_session_expires("  900 ; Refresher = UAS ");
  require(se.valid && se.delta == 900 && se.refresher == Refresher::UAS, "whitespace + case");
  se = parse_session_expires("90");
  require(se.valid && se.delta == 90 && se.refresher == Refresher::NONE, "no refresher");
  se = parse_session_expires("1800;foo=bar;refresher=uac");
  require(se.valid && se.refresher == Refresher::UAC, "other params skipped");
  se = parse_session_expires("1800;refresher=bogus");
  require(se.valid && se.refresher == Refresher::NONE, "unknown refresher ignored");
  require(!parse_session_expires("").valid, "empty");
  require(!parse_session_expires("abc").valid, "non-numeric");
  require(!parse_session_expires(";refresher=uac").valid, "missing delta");
  require(!parse_session_expires("-5").valid, "negative");
}

void test_compact_session_expires_header() {
  // "x:" is the compact form of Session-Expires (RFC 4028 §3).
  SipMessage m = parse_sip_message(
      "INVITE sip:esp@10.0.0.2 SIP/2.0\r\n"
      "x: 1800;refresher=uac\r\n"
      "k: timer, 100rel\r\n"
      "Min-SE: 120\r\n"
      "\r\n");
  require_eq_str(m.header("Session-Expires"), "1800;refresher=uac", "x: maps to Session-Expires");
  require_eq_str(m.header("x"), "1800;refresher=uac", "lookup by compact name");
  require_eq_str(m.header("Supported"), "timer, 100rel", "k: maps to Supported");
  require_eq(parse_min_se(m.header("Min-SE")), 120, "Min-SE");
}

void test_parse_min_se_and_lists() {
  require_eq(parse_min_se("90"), 90, "min-se plain");
  require_eq(parse_min_se(" 120 ;x=y"), 120, "min-se with params");
  require_eq(parse_min_se(""), 0, "min-se empty");
  require_eq(parse_min_se("abc"), 0, "min-se garbage");
  require(list_has_token("replaces, timer", "timer"), "timer in list");
  require(list_has_token("TIMER", "timer"), "case-insensitive");
  require(list_has_token(" 100rel ,  timer ;x", "timer"), "padded + params");
  require(!list_has_token("100rel, replaces", "timer"), "absent");
  require(!list_has_token("timers", "timer"), "prefix is not a match");
  require(!list_has_token("", "timer"), "empty");
}

// ---------------- session timers: decision ----------------

void test_decide_no_header() {
  SessionTimerDecision d = decide_session_timer("", "timer", "");
  require(d.action == SessionTimerDecision::Action::NONE, "no Session-Expires -> nothing to answer");
  d = decide_session_timer("junk", "timer", "");
  require(d.action == SessionTimerDecision::Action::NONE, "unparsable Session-Expires ignored");
}

void test_decide_too_small() {
  SessionTimerDecision d = decide_session_timer("60;refresher=uac", "timer", "");
  require(d.action == SessionTimerDecision::Action::REJECT, "below 90 -> 422");
  require_eq(d.min_se, 90, "422 carries Min-SE 90");
  d = decide_session_timer("89", "", "");
  require(d.action == SessionTimerDecision::Action::REJECT, "89 -> 422");
  d = decide_session_timer("90;refresher=uac", "timer", "");
  require(d.action == SessionTimerDecision::Action::ACCEPT, "exactly 90 is fine");
}

void test_decide_refresher_uac_named() {
  SessionTimerDecision d = decide_session_timer("1800;refresher=uac", "timer", "");
  require(d.action == SessionTimerDecision::Action::ACCEPT, "accept");
  require_eq(d.interval, 1800, "interval echoed");
  require(d.refresher == Refresher::UAC, "uac kept");
  require(d.require_timer, "Supported: timer -> Require: timer");
  require(d.peer_refreshes(), "peer refreshes");
}

void test_decide_refresher_unnamed() {
  SessionTimerDecision d = decide_session_timer("1800", "timer", "");
  require(d.action == SessionTimerDecision::Action::ACCEPT && d.refresher == Refresher::UAC,
          "UAC supports timer -> uac refreshes");
  require(d.require_timer, "Require: timer");
  d = decide_session_timer("1800", "100rel", "");
  require(d.action == SessionTimerDecision::Action::NONE, "UAC without timer support -> we would refresh -> omit");
  d = decide_session_timer("1800", "", "timer");
  require(d.action == SessionTimerDecision::Action::ACCEPT && d.refresher == Refresher::UAC,
          "Require: timer in the request counts as support");
}

void test_decide_refresher_uas_named() {
  // The peer wants us (UAS) to refresh. We never do: leave the header out
  // rather than promise a refresh that would never come.
  SessionTimerDecision d = decide_session_timer("1800;refresher=uas", "timer", "");
  require(d.action == SessionTimerDecision::Action::NONE, "refresher=uas -> not accepted");
  require(!d.peer_refreshes(), "no watchdog without an accepted timer");
}

void test_decide_uac_without_support_flag() {
  SessionTimerDecision d = decide_session_timer("1800;refresher=uac", "", "");
  require(d.action == SessionTimerDecision::Action::ACCEPT, "explicit uac without Supported");
  require(!d.require_timer, "no Require: timer when the UAC never said it supports it");
}

void test_peer_refreshes_in_response() {
  require(peer_refreshes_in_response(parse_session_expires("1800;refresher=uas")), "2xx refresher=uas: peer");
  require(!peer_refreshes_in_response(parse_session_expires("1800;refresher=uac")), "2xx refresher=uac: us");
  require(!peer_refreshes_in_response(parse_session_expires("1800")), "no refresher");
  require(!peer_refreshes_in_response(parse_session_expires("")), "no header");
}

void test_session_watchdog_ms() {
  // RFC 4028 §10: expiry - min(32 s, interval / 3).
  require_eq(session_watchdog_ms(1800), (1800 - 32) * 1000, "1800 s: 32 s margin");
  require_eq(session_watchdog_ms(90), (90 - 30) * 1000, "90 s: 1/3 = 30 s margin");
  require_eq(session_watchdog_ms(96), (96 - 32) * 1000, "96 s: 32 s margin");
  require_eq(session_watchdog_ms(3), 2000, "tiny interval");
  require_eq(session_watchdog_ms(0), 0, "zero");
  require(session_watchdog_ms(0xFFFFFFFFu) <= 2147483647u, "huge interval stays in signed range");
}

void test_decide_refresh_without_header_ends_timer() {
  // RFC 4028 §9: a refresh without Session-Expires means the peer dropped the
  // timer. Never echo the old interval, and tell the caller to stop the watchdog.
  SessionTimerDecision d = decide_session_timer("", "timer", "", /*timer_active=*/true);
  require(d.action == SessionTimerDecision::Action::NONE, "no header -> nothing to answer");
  require(d.ends_timer, "agreed timer ends");
  d = decide_session_timer("", "timer", "", /*timer_active=*/false);
  require(!d.ends_timer, "nothing to end when no timer was agreed");
  d = decide_session_timer("junk", "", "", true);
  require(d.ends_timer, "unparsable header counts as absent");
  d = decide_session_timer("1800;refresher=uas", "timer", "", true);
  require(d.action == SessionTimerDecision::Action::NONE && d.ends_timer,
          "peer now wants us to refresh: timer ends too");
  d = decide_session_timer("1800;refresher=uac", "timer", "", true);
  require(d.action == SessionTimerDecision::Action::ACCEPT && !d.ends_timer, "proper refresh keeps it");
}

void test_ack_matches() {
  auto ack = [](const char *call_id, const char *cseq) {
    return req("ACK", "<sip:pbx>;tag=theirs", "<sip:esp>;tag=ours", call_id, cseq);
  };
  require(ack_matches("call-1@pbx", ack("call-1@pbx", "5 ACK"), 5), "same call, same CSeq");
  require(!ack_matches("call-1@pbx", ack("call-1@pbx", "6 ACK"), 5), "other CSeq");
  require(!ack_matches("call-1@pbx", ack("other@pbx", "5 ACK"), 5), "ACK of another call (e.g. for a 486)");
  require(!ack_matches("call-1@pbx", ack("call-1@pbx", "junk"), 5), "unparsable CSeq");
  require(!ack_matches("", ack("", "5 ACK"), 5), "no dialog");
}

void test_offer_collides() {
  require(offer_collides(true, false, true), "re-INVITE with offer while our offer is open");
  require(offer_collides(true, false, false), "offer-less re-INVITE too");
  require(offer_collides(true, true, true), "UPDATE with SDP");
  require(!offer_collides(true, true, false), "body-less UPDATE is fine");
  require(!offer_collides(false, false, true), "nothing pending");
}

void test_early_outbound_dialog_match() {
  // Outbound early dialog: the peer's tag is not stored yet.
  DialogState st;
  st.call_id = "call-1@pbx";
  st.local_tag = "ours";
  st.remote_tag_unknown = true;
  auto upd = req("UPDATE", "<sip:pbx>;tag=whatever", "<sip:esp>;tag=ours", "call-1@pbx", "1 UPDATE");
  require(classify_dialog_request(st, upd) == DialogVerdict::OK, "matches on Call-ID + our tag");
  auto wrong = req("UPDATE", "<sip:pbx>;tag=whatever", "<sip:esp>;tag=nope", "call-1@pbx", "1 UPDATE");
  require(classify_dialog_request(st, wrong) == DialogVerdict::NOT_IN_DIALOG, "our tag must still match");
  st.remote_tag_unknown = false;
  require(classify_dialog_request(st, upd) == DialogVerdict::NOT_IN_DIALOG, "known dialog needs the remote tag");
}

void test_codec_choice_keeps_current() {
  const std::vector<AudioCodecId> prefs = {AudioCodecId::G722, AudioCodecId::PCMU, AudioCodecId::PCMA};
  SdpInfo both = parse_sdp("c=IN IP4 1.2.3.4\r\nm=audio 4000 RTP/AVP 0 8 9\r\n");
  ChosenAudio fresh = choose_audio_codec(both, prefs);
  require(fresh.pt == 9 && fresh.id == AudioCodecId::G722, "preference order picks g722");
  ChosenAudio keep = choose_audio_codec_keeping(both, prefs, 8, (int) AudioCodecId::PCMA);
  require(keep.pt == 8 && keep.id == AudioCodecId::PCMA, "pcma in use stays");
  SdpInfo no_pcma = parse_sdp("c=IN IP4 1.2.3.4\r\nm=audio 4000 RTP/AVP 0 9\r\n");
  ChosenAudio moved = choose_audio_codec_keeping(no_pcma, prefs, 8, (int) AudioCodecId::PCMA);
  require(moved.pt == 9, "current codec gone: re-choose by preference");
  SdpInfo other_pt = parse_sdp("c=IN IP4 1.2.3.4\r\nm=audio 4000 RTP/AVP 97 9\r\na=rtpmap:97 PCMA/8000\r\n");
  ChosenAudio pt_changed = choose_audio_codec_keeping(other_pt, prefs, 8, (int) AudioCodecId::PCMA);
  require(pt_changed.pt == 9, "same codec on another PT is not 'the same': re-choose");
  SdpInfo none = parse_sdp("c=IN IP4 1.2.3.4\r\nm=audio 4000 RTP/AVP 96\r\na=rtpmap:96 opus/48000/2\r\n");
  require(choose_audio_codec_keeping(none, prefs, 8, (int) AudioCodecId::PCMA).pt < 0, "no common codec");
  // A codec we do not have configured is never kept.
  const std::vector<AudioCodecId> only_u = {AudioCodecId::PCMU};
  require(choose_audio_codec_keeping(both, only_u, 8, (int) AudioCodecId::PCMA).pt == 0,
          "unconfigured current codec is not kept");
}

}  // namespace

int main() {
  test_header_tag_variants();
  test_header_tag_ignores_lookalikes();
  test_parse_cseq();
  test_dialog_match();
  test_dialog_cseq_rules();
  test_direction_names();
  test_answer_direction_full_duplex();
  test_answer_direction_capability();
  test_answer_direction_legacy_hold();
  test_diff_media();
  test_usable_audio();
  test_parse_session_expires();
  test_compact_session_expires_header();
  test_parse_min_se_and_lists();
  test_decide_no_header();
  test_decide_too_small();
  test_decide_refresher_uac_named();
  test_decide_refresher_unnamed();
  test_decide_refresher_uas_named();
  test_decide_uac_without_support_flag();
  test_peer_refreshes_in_response();
  test_decide_refresh_without_header_ends_timer();
  test_ack_matches();
  test_offer_collides();
  test_early_outbound_dialog_match();
  test_codec_choice_keeps_current();
  test_session_watchdog_ms();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "All sip_dialog tests passed\n";
  return 0;
}
