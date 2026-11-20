#pragma once
#include <memory>
#include <string>
#include <vector>
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#ifdef USE_MICROPHONE
#include "esphome/components/microphone/microphone_source.h"
#endif
#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif
#include "codec.h"
#include "rtp_session.h"
#include "sip_dialog.h"
#include "sip_message.h"
#include "sip_transport.h"

namespace esphome {
namespace sip_client {

enum SipAudioChannel : uint8_t {
  SIP_CH_MONO = 0,    // single-channel output (e.g. es8311 mono codec)
  SIP_CH_STEREO = 1,  // duplicate call audio to L/R (mixer / stereo DAC chains)
};

enum SipTransportKind : uint8_t {
  SIP_TRANSPORT_UDP = 0,
  SIP_TRANSPORT_WS = 1,   // RFC 7118, plain
  SIP_TRANSPORT_WSS = 2,  // RFC 7118 over TLS
};

enum SipState {
  SIP_IDLE = 0,
  SIP_REGISTERING,
  SIP_REGISTERED,
  SIP_INVITING,    // outbound INVITE sent, awaiting answer
  SIP_RINGING_OUT,  // outbound, got 180
  SIP_INCOMING,    // inbound INVITE, ringing locally
  SIP_ANSWERING,   // inbound, sent 200, awaiting ACK
  SIP_IN_CALL,
};

class SipClient : public Component {
 public:
#ifdef USE_MICROPHONE
  void set_microphone_source(microphone::MicrophoneSource *mic_source) { this->mic_source_ = mic_source; }
#endif
#ifdef USE_SPEAKER
  void set_speaker(speaker::Speaker *spk) { this->speaker_ = spk; }
#endif
  void set_server(const std::string &server) { this->server_ = server; }
  void set_port(uint16_t port) { this->server_port_ = port; }
  void set_transport(SipTransportKind t) { this->transport_kind_ = t; }
  void set_ws_path(const std::string &v) { this->ws_path_ = v; }
  void set_verify_ssl(bool v) { this->verify_ssl_ = v; }
  void set_ca_certificate(const std::string &v) { this->ca_certificate_ = v; }
  void set_username(const std::string &v) { this->username_ = v; }
  // Digest authentication user, when the PBX separates it from the extension
  // (3CX calls it the Authentication ID). Empty = authenticate as `username`.
  void set_auth_username(const std::string &v) { this->auth_username_ = v; }
  void set_password(const std::string &v) { this->password_ = v; }
  void set_domain(const std::string &v) { this->domain_ = v; }
  void set_caller_id(const std::string &v) { this->caller_id_ = v; }
  void set_register_expiration(uint32_t s) { this->expiration_ = s; }
  void set_local_rtp_port(uint16_t p) { this->local_rtp_port_ = p; }
  // Idle RTP silence interval in ms (0 = continuous 20 ms silence while idle).
  void set_rtp_keepalive(uint32_t ms) { this->rtp_keepalive_ms_ = ms; }
  void set_channel(SipAudioChannel c) { this->channel_ = c; }
  void set_codecs(const std::vector<AudioCodecId> &codecs) { this->configured_codecs_ = codecs; }

  void add_on_registered_callback(std::function<void()> &&cb) { this->registered_cb_.add(std::move(cb)); }
  void add_on_incoming_call_callback(std::function<void(std::string)> &&cb) {
    this->incoming_call_cb_.add(std::move(cb));
  }
  void add_on_call_connected_callback(std::function<void()> &&cb) { this->connected_cb_.add(std::move(cb)); }
  void add_on_call_ended_callback(std::function<void()> &&cb) { this->ended_cb_.add(std::move(cb)); }
  void add_on_dtmf_callback(std::function<void(std::string)> &&cb) { this->dtmf_cb_.add(std::move(cb)); }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  void set_half_duplex(bool v) { this->half_duplex_ = v; }

  // Call-control API (used by automation actions).
  void call(const std::string &number);
  void answer();
  void hangup();
  void send_dtmf(const std::string &digits);

  // Push-to-talk (half-duplex). On boards that cannot capture and play at the
  // same time (single shared I2S bus, e.g. Atom Echo), the mic and speaker are
  // never active together: the call starts in receive (speaker) mode and these
  // switch the single bus between transmit and receive. No-ops in full duplex.
  void start_talking();
  void stop_talking();
  bool is_talking() const { return this->talking_; }

  bool in_call() const { return this->state_ == SIP_IN_CALL; }

 protected:
  // networking
  void send_raw_(const std::string &msg);
  void handle_packet_(const std::string &raw);
  void handle_request_(const SipMessage &m, const std::string &raw);
  void handle_register_response_(const SipMessage &m);
  void handle_invite_response_(const SipMessage &m, const std::string &raw);

  // message builders
  std::string build_register_();
  std::string build_invite_();
  std::string build_ack_(const SipMessage &resp);
  std::string build_request_in_dialog_(const std::string &method);
  // `extra_headers` (complete "Name: value\r\n" lines) are added before the
  // body; `sdp_body` overrides the default answer SDP when with_sdp is set.
  std::string build_response_(const SipMessage &req, int code, const std::string &reason,
                              bool with_sdp, const std::string &extra_headers = std::string(),
                              const std::string &sdp_body = std::string());
  // Offer (answer=false): all supported codecs. Answer (true): chosen codec
  // only + telephone-event when the remote offered it (RFC 3264 §6.1).
  // `direction` defaults to the negotiated one (local_dir_).
  std::string local_sdp_(bool answer = false, const char *direction = nullptr);
  void apply_chosen_codec_(int pt, AudioCodecId id);
  std::string contact_uri_();
  // Whole "Via: ...\r\n" line built from the transport (token, sent-by, rport).
  std::string via_header_(const std::string &branch);

  // in-dialog requests (re-INVITE / UPDATE, RFC 3261 §14, RFC 3311)
  DialogState dialog_state_() const;
  void handle_in_dialog_request_(const SipMessage &m, DialogVerdict verdict);
  void handle_ack_(const SipMessage &m);
  // Apply a remote SDP (re-offer, or the answer in an ACK) to the running call:
  // codec, RTP endpoint, DTMF PT, direction / hold. `offered` is the direction
  // capability the answer is intersected with. Returns false and changes
  // nothing when the SDP has no usable audio / common codec.
  bool apply_remote_sdp_(const SdpInfo &sdp, SdpDirection offered);
  // Initial offer/answer: direction + send/receive gating (RFC 3264 §6.1).
  void apply_initial_direction_(const SdpInfo &sdp);
  // Store + send a final response to an INVITE / UPDATE (kept for retransmits).
  void send_final_response_(const SipMessage &m, uint32_t cseq, const std::string &resp);
  void send_retry_later_(const SipMessage &m, uint32_t cseq);
  // Everything that belongs to the current call but not to the dialog ids:
  // remote CSeq, stored response, pending ACK, SDP version, hold state, timers.
  void reset_call_session_();
  SdpDirection capability_direction_() const { return direction_from_name(this->media_direction_()); }

  // session timers (RFC 4028): the peer refreshes, we only watch.
  std::string commit_session_timer_(const SessionTimerDecision &d);
  bool reinvite_offer_pending_() const;
  void arm_session_watchdog_();

  // registration
  void do_register_();
  // The signaling connection went away (TCP/TLS drop or reconnect): end any
  // call, forget the registration and schedule a fresh REGISTER.
  void handle_link_down_();

  // media
  void start_media_();
  void stop_media_();
  void start_speaker_();
  void stop_speaker_();
  void start_mic_();
  void stop_mic_();
#ifdef USE_MICROPHONE
  void on_mic_data_(const std::vector<uint8_t> &data);
#endif
  // SDP / logs: based on which audio endpoints are wired.
  // USE_MICROPHONE / USE_SPEAKER are set by ESPHome when any mic/speaker
  // platform is in the build; pointers may still be null if this component
  // did not take a reference (send-only / receive-only).
  const char *media_direction_() const {
#if defined(USE_MICROPHONE) && defined(USE_SPEAKER)
    if (this->mic_source_ == nullptr)
      return "recvonly";
    if (this->speaker_ == nullptr)
      return "sendonly";
    return "sendrecv";
#elif defined(USE_SPEAKER)
    return "recvonly";
#elif defined(USE_MICROPHONE)
    return "sendonly";
#else
    return "inactive";
#endif
  }

  void set_state_(SipState s);
  std::string extract_caller_(const SipMessage &m);

  // config
#ifdef USE_MICROPHONE
  microphone::MicrophoneSource *mic_source_{nullptr};
#endif
#ifdef USE_SPEAKER
  speaker::Speaker *speaker_{nullptr};
#endif
  std::string server_;
  uint16_t server_port_{5060};
  SipTransportKind transport_kind_{SIP_TRANSPORT_UDP};
  std::string ws_path_{"/ws"};
  bool verify_ssl_{true};
  std::string ca_certificate_;
  std::string username_;
  std::string auth_username_;
  std::string password_;
  std::string domain_;
  std::string caller_id_;
  // From / To / Contact always stay on username_; only Digest uses this.
  const std::string &auth_user_() const {
    return this->auth_username_.empty() ? this->username_ : this->auth_username_;
  }
  uint32_t expiration_{300};
  uint16_t local_rtp_port_{7078};
  uint32_t rtp_keepalive_ms_{0};
  SipAudioChannel channel_{SIP_CH_STEREO};
  uint8_t output_channels_() const { return this->channel_ == SIP_CH_MONO ? 1 : 2; }
  bool half_duplex_{false};
  bool talking_{false};  // half-duplex: true = transmitting (mic), false = receiving (speaker)

  // runtime networking
  std::unique_ptr<SipTransport> transport_;
  bool transport_up_{false};  // loop() saw the transport connected and sent REGISTER on it
  uint32_t transport_generation_{0};  // transport_->generation() when transport_up_ was set

  SipState state_{SIP_IDLE};
  bool registered_{false};
  uint32_t next_register_ms_{0};
  bool register_auth_tried_{false};

  // registration transaction identifiers
  std::string reg_call_id_;
  std::string reg_tag_;
  std::string reg_branch_;
  uint32_t reg_cseq_{0};

  // current dialog
  std::string d_call_id_;
  std::string d_local_;        // our From-style header incl. tag
  std::string d_remote_;       // peer header incl. tag
  std::string d_remote_target_;  // request-URI for in-dialog requests
  std::string d_invite_uri_;     // Request-URI of our INVITE (non-2xx ACK / CANCEL target)
  std::vector<std::string> dialog_routes_;  // route set (RFC 3261 §12.1), first hop first
  std::string d_local_tag_;
  std::string d_branch_;       // branch of the INVITE transaction
  uint32_t d_cseq_{0};         // our own CSeq counter (BYE, ...)
  uint32_t d_remote_cseq_{0};  // highest CSeq processed from the peer (RFC 3261 §12.2.2)
  uint32_t d_initial_cseq_{0};  // inbound: CSeq of the INVITE we answered (matches its ACK)
  bool outbound_{false};       // true if we initiated the call
  bool invite_auth_tried_{false};
  std::string invite_number_;
  SipMessage incoming_invite_;  // stored for building responses (inbound)
  std::string incoming_invite_raw_;

  // negotiated media
  std::string remote_rtp_ip_;
  uint16_t remote_rtp_port_{0};
  int chosen_pt_{-1};                     // negotiated wire PT, -1 if none
  const char *chosen_rtpmap_{nullptr};    // SDP label, e.g. "PCMA/8000"
  std::unique_ptr<Codec> active_codec_;   // encode/decode for the call
  int remote_dtmf_pt_{-1};

  // in-dialog offer/answer state
  SdpDirection local_dir_{SdpDirection::SENDRECV};  // direction we advertise (negotiated)
  bool tx_allowed_{true};  // the peer takes media from us (false while it holds us)
  bool rx_allowed_{true};  // the peer sends media to us
  std::string sdp_session_id_;   // o= session id, fixed for the whole call
  uint32_t sdp_version_{0};      // o= version, bumped when our SDP body changes
  std::string sdp_signature_;    // our last SDP body with a neutral version
  std::string last_response_;    // final response to the last INVITE / UPDATE (retransmits)
  uint32_t last_response_cseq_{0};
  std::string last_response_method_;
  bool reinvite_ack_pending_{false};  // our 2xx to a re-INVITE awaits its ACK
  uint32_t reinvite_ack_cseq_{0};
  bool reinvite_offerless_{false};    // that ACK carries the SDP answer
  uint32_t reinvite_ack_ms_{0};       // give up waiting for that ACK at this time

  // session timer (RFC 4028) with the peer as refresher
  bool session_timer_active_{false};  // agreed with the peer
  bool session_armed_{false};         // watchdog running (call is up)
  uint32_t session_interval_s_{0};
  uint32_t session_deadline_ms_{0};
  std::string session_resp_headers_;  // Session-Expires etc. for the initial 200 OK
  std::vector<AudioCodecId> configured_codecs_{AudioCodecId::PCMU, AudioCodecId::PCMA};

  RtpSession rtp_;
  uint32_t mic_rate_{16000};
  uint8_t mic_channels_{1};
  uint8_t mic_bits_{16};
  bool media_active_{false};

  CallbackManager<void()> registered_cb_{};
  CallbackManager<void(std::string)> incoming_call_cb_{};
  CallbackManager<void()> connected_cb_{};
  CallbackManager<void()> ended_cb_{};
  CallbackManager<void(std::string)> dtmf_cb_{};
};

}  // namespace sip_client
}  // namespace esphome
