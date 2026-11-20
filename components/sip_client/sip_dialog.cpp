#include "sip_dialog.h"
#include <cctype>
#include <cstdlib>

namespace esphome {
namespace sip_client {

namespace {

std::string lower(std::string s) {
  for (auto &c : s) c = (char) std::tolower((unsigned char) c);
  return s;
}

std::string trim_ws(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

bool all_digits(const std::string &s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!std::isdigit((unsigned char) c)) return false;
  }
  return true;
}

// Parse a run of digits into a uint32 (clamped). False when not all digits.
bool parse_u32(const std::string &s, uint32_t &out) {
  if (!all_digits(s) || s.size() > 10) return false;
  unsigned long long v = std::strtoull(s.c_str(), nullptr, 10);
  out = v > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t) v;
  return true;
}

// Split on ';' outside double quotes.
std::vector<std::string> split_params(const std::string &s) {
  std::vector<std::string> out;
  size_t start = 0;
  bool quoted = false;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '"') {
      quoted = !quoted;
    } else if (s[i] == '\\' && quoted) {
      i++;
    } else if (s[i] == ';' && !quoted) {
      out.push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  out.push_back(s.substr(start));
  return out;
}

}  // namespace

// ---------------- dialog identification ----------------

std::string header_tag(const std::string &value) {
  // Find where the header parameters start.
  size_t params = std::string::npos;
  bool quoted = false;
  size_t lt = std::string::npos;
  for (size_t i = 0; i < value.size(); i++) {
    char c = value[i];
    if (quoted && c == '\\') {
      i++;
    } else if (c == '"') {
      quoted = !quoted;
    } else if (!quoted && c == '<') {
      lt = i;
      break;
    }
  }
  if (lt != std::string::npos) {
    size_t gt = value.find('>', lt);
    if (gt == std::string::npos) return "";
    params = gt + 1;
  } else {
    // addr-spec without brackets: URI parameters are not allowed, so the first
    // ';' already starts the header parameters.
    params = value.find(';');
    if (params == std::string::npos) return "";
  }
  const std::string tail = value.substr(params);
  for (const auto &raw : split_params(tail)) {
    std::string p = trim_ws(raw);
    size_t eq = p.find('=');
    if (eq == std::string::npos) continue;
    if (lower(trim_ws(p.substr(0, eq))) != "tag") continue;
    std::string v = trim_ws(p.substr(eq + 1));
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    return v;
  }
  return "";
}

bool parse_cseq(const std::string &value, uint32_t &number, std::string &method) {
  std::string v = trim_ws(value);
  size_t sp = v.find_first_of(" \t");
  if (sp == std::string::npos) return false;
  if (!parse_u32(v.substr(0, sp), number)) return false;
  method = trim_ws(v.substr(sp + 1));
  return !method.empty();
}

DialogVerdict classify_dialog_request(const DialogState &st, const SipMessage &req) {
  const std::string to_tag = header_tag(req.header("To"));
  if (to_tag.empty()) return DialogVerdict::NEW_DIALOG;

  const std::string from_tag = header_tag(req.header("From"));
  if (st.call_id.empty() || st.local_tag.empty() || req.header("Call-ID") != st.call_id ||
      to_tag != st.local_tag) {
    return DialogVerdict::NOT_IN_DIALOG;
  }
  if (!st.remote_tag_unknown && (st.remote_tag.empty() || from_tag != st.remote_tag))
    return DialogVerdict::NOT_IN_DIALOG;

  uint32_t cseq = 0;
  std::string cseq_method;
  if (!parse_cseq(req.header("CSeq"), cseq, cseq_method)) return DialogVerdict::STALE_CSEQ;

  if (st.last_response_cseq != 0 && cseq == st.last_response_cseq &&
      cseq_method == st.last_response_method) {
    return DialogVerdict::RETRANSMISSION;
  }
  // RFC 3261 §12.2.2: a request whose CSeq is not above the remote sequence
  // number is out of order.
  if (cseq <= st.remote_cseq) return DialogVerdict::STALE_CSEQ;
  return DialogVerdict::OK;
}

bool ack_matches(const std::string &call_id, const SipMessage &ack, uint32_t expected_cseq) {
  uint32_t cseq = 0;
  std::string method;
  if (call_id.empty() || ack.header("Call-ID") != call_id) return false;
  if (!parse_cseq(ack.header("CSeq"), cseq, method)) return false;
  return cseq == expected_cseq;
}

bool offer_collides(bool our_offer_unanswered, bool is_update, bool has_body) {
  return our_offer_unanswered && (!is_update || has_body);
}

// ---------------- SDP direction ----------------

const char *direction_name(SdpDirection d) {
  switch (d) {
    case SdpDirection::SENDRECV:
      return "sendrecv";
    case SdpDirection::SENDONLY:
      return "sendonly";
    case SdpDirection::RECVONLY:
      return "recvonly";
    case SdpDirection::INACTIVE:
      return "inactive";
  }
  return "sendrecv";
}

SdpDirection direction_from_name(const std::string &name) {
  const std::string n = lower(trim_ws(name));
  if (n == "sendonly") return SdpDirection::SENDONLY;
  if (n == "recvonly") return SdpDirection::RECVONLY;
  if (n == "inactive") return SdpDirection::INACTIVE;
  return SdpDirection::SENDRECV;
}

SdpDirection direction_from_flags(bool send, bool receive) {
  if (send && receive) return SdpDirection::SENDRECV;
  if (send) return SdpDirection::SENDONLY;
  if (receive) return SdpDirection::RECVONLY;
  return SdpDirection::INACTIVE;
}

SdpDirection answer_direction(SdpDirection offer, bool offer_on_hold, SdpDirection capability) {
  const bool offerer_receives = direction_receives(offer) && !offer_on_hold;
  const bool offerer_sends = direction_sends(offer);
  return direction_from_flags(offerer_receives && direction_sends(capability),
                              offerer_sends && direction_receives(capability));
}

// ---------------- codec choice ----------------

namespace {

ChosenAudio audio_for(const SdpInfo &sdp, AudioCodecId id) {
  switch (id) {
    case AudioCodecId::PCMU:
      if (sdp.pcmu_pt >= 0) return {sdp.pcmu_pt, AudioCodecId::PCMU, "PCMU/8000"};
      break;
    case AudioCodecId::PCMA:
      if (sdp.pcma_pt >= 0) return {sdp.pcma_pt, AudioCodecId::PCMA, "PCMA/8000"};
      break;
    case AudioCodecId::G722:
      if (sdp.g722_pt >= 0) return {sdp.g722_pt, AudioCodecId::G722, "G722/8000"};
      break;
  }
  return {};
}

}  // namespace

ChosenAudio choose_audio_codec(const SdpInfo &sdp, const std::vector<AudioCodecId> &prefs) {
  for (AudioCodecId id : prefs) {
    ChosenAudio c = audio_for(sdp, id);
    if (c.pt >= 0) return c;
  }
  return {};
}

ChosenAudio choose_audio_codec_keeping(const SdpInfo &sdp, const std::vector<AudioCodecId> &prefs,
                                       int current_pt, int current_codec_id) {
  for (AudioCodecId id : prefs) {
    if (static_cast<int>(id) != current_codec_id) continue;
    ChosenAudio c = audio_for(sdp, id);
    if (c.pt >= 0 && c.pt == current_pt) return c;
  }
  return choose_audio_codec(sdp, prefs);
}

// ---------------- re-offer diffing ----------------

bool ip_is_ipv6(const std::string &ip) { return ip.find(':') != std::string::npos; }

MediaDelta diff_media(const NegotiatedMedia &cur, const NegotiatedMedia &next) {
  MediaDelta d;
  d.codec_changed = cur.pt != next.pt || cur.codec_id != next.codec_id;
  d.remote_changed = cur.ip != next.ip || cur.port != next.port;
  d.dtmf_changed = cur.dtmf_pt != next.dtmf_pt;
  d.family_changed = ip_is_ipv6(cur.ip) != ip_is_ipv6(next.ip);
  return d;
}

// ---------------- session timers ----------------

SessionExpires parse_session_expires(const std::string &value) {
  SessionExpires se;
  const auto parts = split_params(value);
  if (parts.empty()) return se;
  uint32_t delta = 0;
  if (!parse_u32(trim_ws(parts[0]), delta)) return se;
  se.delta = delta;
  se.valid = true;
  for (size_t i = 1; i < parts.size(); i++) {
    std::string p = trim_ws(parts[i]);
    size_t eq = p.find('=');
    if (eq == std::string::npos) continue;
    if (lower(trim_ws(p.substr(0, eq))) != "refresher") continue;
    const std::string v = lower(trim_ws(p.substr(eq + 1)));
    if (v == "uac") se.refresher = Refresher::UAC;
    else if (v == "uas") se.refresher = Refresher::UAS;
  }
  return se;
}

uint32_t parse_min_se(const std::string &value) {
  const auto parts = split_params(value);
  uint32_t delta = 0;
  if (parts.empty() || !parse_u32(trim_ws(parts[0]), delta)) return 0;
  return delta;
}

bool list_has_token(const std::string &value, const std::string &token) {
  const std::string want = lower(token);
  size_t start = 0;
  while (start <= value.size()) {
    size_t comma = value.find(',', start);
    std::string item = value.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    size_t semi = item.find(';');
    if (semi != std::string::npos) item = item.substr(0, semi);
    if (lower(trim_ws(item)) == want) return true;
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return false;
}

SessionTimerDecision decide_session_timer(const std::string &session_expires,
                                          const std::string &supported, const std::string &require,
                                          bool timer_active, uint32_t our_min_se) {
  SessionTimerDecision d;
  const SessionExpires se = parse_session_expires(session_expires);
  if (!se.valid) {
    // Absent or unparsable: behave as without timers. A timer agreed earlier
    // ends here (RFC 4028 §9) - never keep a watchdog the peer will not feed.
    d.ends_timer = timer_active;
    return d;
  }

  if (se.delta < our_min_se) {
    d.action = SessionTimerDecision::Action::REJECT;
    d.min_se = our_min_se;
    return d;
  }

  const bool uac_supports = list_has_token(supported, "timer") || list_has_token(require, "timer");
  Refresher refresher = se.refresher;
  if (refresher == Refresher::NONE)
    refresher = uac_supports ? Refresher::UAC : Refresher::UAS;
  // We never refresh. If we would be the refresher (the peer asked for uas, or
  // it does not support timers so nobody else could), act like a UA that does
  // not know the extension and leave Session-Expires out of the 2xx (§8.2).
  if (refresher != Refresher::UAC) {
    d.ends_timer = timer_active;
    return d;
  }

  d.action = SessionTimerDecision::Action::ACCEPT;
  d.interval = se.delta;
  d.refresher = Refresher::UAC;
  d.require_timer = uac_supports;
  return d;
}

bool peer_refreshes_in_response(const SessionExpires &se) {
  return se.valid && se.refresher == Refresher::UAS;
}

uint32_t session_watchdog_ms(uint32_t interval_s) {
  // Keep the result inside a signed 32-bit millisecond range so callers can
  // compare deadlines wrap-safely.
  const uint32_t kMaxSeconds = 2000000;
  if (interval_s > kMaxSeconds) interval_s = kMaxSeconds;
  uint32_t margin = interval_s / 3;
  if (margin > 32) margin = 32;
  return (interval_s - margin) * 1000u;
}

}  // namespace sip_client
}  // namespace esphome
