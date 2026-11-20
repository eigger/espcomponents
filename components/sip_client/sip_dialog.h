#pragma once
#include <cstdint>
#include <string>
#include "codec.h"
#include "sip_message.h"

namespace esphome {
namespace sip_client {

// Host-testable in-dialog logic (no ESPHome runtime dependency): dialog
// matching (RFC 3261 §12.2.2), SDP direction answers (RFC 3264 §6.1 / §8.4),
// re-offer diffing and session timers (RFC 4028).

// ---------------- dialog identification ----------------

// "tag" parameter of a From / To field-value, or "" when absent. A name-addr
// takes its header parameters after the closing '>' (a tag= inside the quoted
// display name or inside the URI's own parameters is not ours); a bare addr-spec
// has no URI parameters, so everything after the first ';' is a header param.
std::string header_tag(const std::string &value);

// CSeq header value "314159 INVITE" -> number + method. False when malformed.
bool parse_cseq(const std::string &value, uint32_t &number, std::string &method);

// What we know about the one dialog we can have.
struct DialogState {
  std::string call_id;
  std::string local_tag;   // our tag (the To tag of requests we receive)
  std::string remote_tag;  // peer's tag (the From tag of requests we receive)
  // Early dialog of an outbound call: the peer's tag is only learned from the 2xx,
  // so requests are matched on Call-ID + our tag alone.
  bool remote_tag_unknown{false};
  uint32_t remote_cseq{0};  // highest CSeq processed from the peer
  // Final response we last sent to an INVITE / UPDATE, for retransmissions.
  uint32_t last_response_cseq{0};  // 0 = none stored
  std::string last_response_method;
};

enum class DialogVerdict : uint8_t {
  NEW_DIALOG,       // no To tag: a dialog-creating request
  NOT_IN_DIALOG,    // To tag present but not ours -> 481
  RETRANSMISSION,   // same CSeq + method as the stored final response -> resend it
  STALE_CSEQ,       // CSeq not above the last one (or unparsable) -> 500 (§12.2.2)
  OK,               // new in-dialog transaction
};

DialogVerdict classify_dialog_request(const DialogState &st, const SipMessage &req);

// An ACK belongs to the transaction we wait for: same Call-ID and the CSeq
// number of the INVITE (the answered initial INVITE, or the re-INVITE).
bool ack_matches(const std::string &call_id, const SipMessage &ack, uint32_t expected_cseq);

// RFC 3261 §14.1 / RFC 6337: while our 200 to an offer-less re-INVITE carries an
// offer whose answer (in the ACK) has not arrived, a new offer (re-INVITE, or an
// UPDATE with SDP) gets 491 Request Pending. A body-less UPDATE does not.
bool offer_collides(bool our_offer_unanswered, bool is_update, bool has_body);

// ---------------- SDP direction ----------------

const char *direction_name(SdpDirection d);
// Unknown names fall back to sendrecv.
SdpDirection direction_from_name(const std::string &name);
inline bool direction_sends(SdpDirection d) {
  return d == SdpDirection::SENDRECV || d == SdpDirection::SENDONLY;
}
inline bool direction_receives(SdpDirection d) {
  return d == SdpDirection::SENDRECV || d == SdpDirection::RECVONLY;
}
SdpDirection direction_from_flags(bool send, bool receive);

// Direction to answer an offer with (RFC 3264 §6.1): we send only when the
// offerer receives and we can send, and receive only when the offerer sends
// and we can receive. `offer_on_hold` (c=0.0.0.0) means the offerer takes no
// media, i.e. we must not send either (§8.4). `capability` is what the device
// can do (no microphone -> recvonly, ...).
SdpDirection answer_direction(SdpDirection offer, bool offer_on_hold, SdpDirection capability);

// ---------------- codec choice ----------------

struct ChosenAudio {
  int pt{-1};
  AudioCodecId id{AudioCodecId::PCMU};
  const char *rtpmap{nullptr};
};

// First configured codec (in preference order) the SDP offers; pt < 0 if none.
// Encoding follows the rtpmap name / codec id, not the wire PT number.
ChosenAudio choose_audio_codec(const SdpInfo &sdp, const std::vector<AudioCodecId> &prefs);
// Same, but a re-offer that still contains the codec in use (same PT) keeps it,
// so a re-anchor / transfer does not restart the audio for a preference change.
ChosenAudio choose_audio_codec_keeping(const SdpInfo &sdp, const std::vector<AudioCodecId> &prefs,
                                       int current_pt, int current_codec_id);

// ---------------- re-offer diffing ----------------

struct NegotiatedMedia {
  int pt{-1};
  int codec_id{-1};  // AudioCodecId as int
  std::string ip;
  uint16_t port{0};
  int dtmf_pt{-1};
};

struct MediaDelta {
  bool codec_changed{false};   // wire PT or codec differs
  bool remote_changed{false};  // RTP address or port differs
  bool dtmf_changed{false};
  bool family_changed{false};  // IPv4 <-> IPv6: the RTP socket must be recreated
  // The RTP session has to be stopped and started again (a codec object can
  // only be replaced while media is stopped; a socket has one address family).
  bool restart_needed() const { return this->codec_changed || this->family_changed; }
  bool any() const {
    return this->codec_changed || this->remote_changed || this->dtmf_changed || this->family_changed;
  }
};

MediaDelta diff_media(const NegotiatedMedia &cur, const NegotiatedMedia &next);

// Whether a remote address is IPv6 (contains ':').
bool ip_is_ipv6(const std::string &ip);

// A re-offer carries usable RTP: valid audio section, non-zero port.
inline bool sdp_has_usable_audio(const SdpInfo &sdp) { return sdp.valid && sdp.audio_port != 0; }

// ---------------- session timers (RFC 4028) ----------------

enum class Refresher : uint8_t { NONE, UAC, UAS };

struct SessionExpires {
  bool valid{false};
  uint32_t delta{0};
  Refresher refresher{Refresher::NONE};
};

// "Session-Expires: 1800;refresher=uac" (compact "x:" is mapped by the header
// parser). Tolerant of case and whitespace.
SessionExpires parse_session_expires(const std::string &value);
// Min-SE delta-seconds, 0 when absent / malformed.
uint32_t parse_min_se(const std::string &value);
// Whether a comma-separated option-tag list (Supported / Require) has `token`.
bool list_has_token(const std::string &value, const std::string &token);

constexpr uint32_t SESSION_MIN_SE = 90;  // RFC 4028 §4 lower bound

struct SessionTimerDecision {
  enum class Action : uint8_t {
    NONE,    // no session timer for this transaction (header absent / unusable /
             // we would be the refresher, which we never are)
    REJECT,  // 422 Session Interval Too Small
    ACCEPT,  // 2xx carries Session-Expires
  };
  Action action{Action::NONE};
  uint32_t interval{0};         // ACCEPT: seconds for the 2xx
  Refresher refresher{Refresher::NONE};  // ACCEPT: value for the 2xx (relative to this transaction)
  bool require_timer{false};    // ACCEPT: put "Require: timer" in the 2xx
  uint32_t min_se{0};           // REJECT: value for the Min-SE header of the 422
  // ACCEPT: the peer (UAC of this transaction) refreshes; the dialog needs a
  // watchdog. Always true for ACCEPT because we never accept the refresher role.
  bool peer_refreshes() const { return this->action == Action::ACCEPT; }
  // NONE while a timer was agreed: the request dropped it (no Session-Expires,
  // RFC 4028 §9, or one we would have to refresh), so the watchdog must stop.
  bool ends_timer{false};
};

// Decide how to answer an INVITE / UPDATE (initial or refresh). Inputs are the
// raw header values ("" when absent) and whether a timer is currently agreed.
// A refresh without Session-Expires means the peer no longer uses the timer
// (RFC 4028 §9); that is NONE + ends_timer, never an echo of the old interval.
SessionTimerDecision decide_session_timer(const std::string &session_expires,
                                          const std::string &supported, const std::string &require,
                                          bool timer_active = false,
                                          uint32_t our_min_se = SESSION_MIN_SE);

// For a 2xx we received to our own INVITE: whether the peer (UAS) is the
// refresher. A 2xx without a refresher parameter or without the header means
// there is no (usable) session timer from the peer's side.
bool peer_refreshes_in_response(const SessionExpires &se);

// Milliseconds after the last refresh at which a non-refreshing side must give
// up and send BYE: expiry minus min(32 s, interval / 3) (RFC 4028 §10).
uint32_t session_watchdog_ms(uint32_t interval_s);

}  // namespace sip_client
}  // namespace esphome
