#include "sip_client.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/util.h"
#include "esphome/components/audio/audio.h"
#include "audio_resampler.h"
#include "dtmf.h"
#include "g711_codec.h"
#include "g722_codec.h"
#include "sdp_builder.h"
#include "sip_auth.h"
#include "sip_transport_udp.h"
#include "sip_transport_ws.h"

namespace esphome {
namespace sip_client {

static const char *const TAG = "sip_client";
static const char *const USER_AGENT = "ESPHome-sip_client";
// Methods we handle; UPDATE is the RFC 3311 session refresh / hold carrier.
static const char *const ALLOW_METHODS = "INVITE, ACK, CANCEL, BYE, OPTIONS, INFO, UPDATE";

// Session-Expires + Supported (+ Require) lines for a 2xx we send (RFC 4028 §8.2).
// The refresher is always the peer: we never refresh.
static std::string session_timer_headers(uint32_t interval_s, bool require_timer) {
  std::string h = "Session-Expires: " + std::to_string(interval_s) + ";refresher=uac\r\n";
  h += "Supported: timer\r\n";
  if (require_timer) h += "Require: timer\r\n";
  return h;
}

void SipClient::setup() {
  std::srand(micros());
  if (this->domain_.empty()) this->domain_ = this->server_;
#ifdef USE_SIP_CLIENT_WS
  if (this->transport_kind_ != SIP_TRANSPORT_UDP) {
    WsTransportConfig cfg;
    cfg.tls = this->transport_kind_ == SIP_TRANSPORT_WSS;
    cfg.path = this->ws_path_;
    cfg.verify_ssl = this->verify_ssl_;
    cfg.ca_certificate = this->ca_certificate_;
    this->transport_ = std::make_unique<WsTransport>(cfg);
  }
#endif
  if (!this->transport_) {
    if (this->transport_kind_ != SIP_TRANSPORT_UDP)
      ESP_LOGE(TAG, "WebSocket transport requested but not compiled in; falling back to UDP");
    this->transport_ = std::make_unique<UdpTransport>();
  }

#ifdef USE_MICROPHONE
  if (this->mic_source_ != nullptr) {
    this->mic_source_->add_data_callback(
        [this](const std::vector<uint8_t> &data) { this->on_mic_data_(data); });
  }
#endif
  ESP_LOGCONFIG(TAG, "sip_client initialized (server=%s:%u user=%s)", this->server_.c_str(),
                this->server_port_, this->username_.c_str());
}

void SipClient::dump_config() {
  ESP_LOGCONFIG(TAG, "SIP Client:");
  ESP_LOGCONFIG(TAG, "  Server: %s:%u", this->server_.c_str(), this->server_port_);
  ESP_LOGCONFIG(TAG, "  Transport: %s",
                this->transport_kind_ == SIP_TRANSPORT_WSS ? "wss"
                : this->transport_kind_ == SIP_TRANSPORT_WS ? "ws"
                                                            : "udp");
  if (this->transport_kind_ != SIP_TRANSPORT_UDP) {
    ESP_LOGCONFIG(TAG, "  Path: %s", this->ws_path_.c_str());
    if (this->transport_kind_ == SIP_TRANSPORT_WSS) {
      ESP_LOGCONFIG(TAG, "  Verify TLS: %s", this->verify_ssl_ ? "yes" : "NO");
      if (!this->ca_certificate_.empty()) ESP_LOGCONFIG(TAG, "  Custom CA certificate: yes");
    }
  }
  ESP_LOGCONFIG(TAG, "  Username: %s", this->username_.c_str());
  if (!this->auth_username_.empty())
    ESP_LOGCONFIG(TAG, "  Auth username: %s", this->auth_username_.c_str());
  ESP_LOGCONFIG(TAG, "  Domain: %s", this->domain_.c_str());
  ESP_LOGCONFIG(TAG, "  Local RTP port: %u", this->local_rtp_port_);
  if (this->rtp_keepalive_ms_ != 0)
    ESP_LOGCONFIG(TAG, "  RTP keepalive: %u ms while idle", static_cast<unsigned>(this->rtp_keepalive_ms_));
  else
    ESP_LOGCONFIG(TAG, "  RTP keepalive: continuous silence while idle");
  ESP_LOGCONFIG(TAG, "  Channel: %s", this->channel_ == SIP_CH_MONO ? "mono" : "stereo");
  ESP_LOGCONFIG(TAG, "  Half-duplex (PTT): %s", this->half_duplex_ ? "yes" : "no");
  {
    std::string codecs;
    for (AudioCodecId id : this->configured_codecs_) {
      const char *name = "?";
      switch (id) {
        case AudioCodecId::PCMU:
          name = "pcmu";
          break;
        case AudioCodecId::PCMA:
          name = "pcma";
          break;
        case AudioCodecId::G722:
          name = "g722";
          break;
      }
      if (!codecs.empty()) codecs += ", ";
      codecs += name;
    }
    ESP_LOGCONFIG(TAG, "  Codecs: [%s]", codecs.c_str());
  }
#ifdef USE_MICROPHONE
  ESP_LOGCONFIG(TAG, "  Microphone: %s", this->mic_source_ ? "yes" : "no");
#else
  ESP_LOGCONFIG(TAG, "  Microphone: no");
#endif
#ifdef USE_SPEAKER
  ESP_LOGCONFIG(TAG, "  Speaker: %s", this->speaker_ ? "yes" : "no");
#else
  ESP_LOGCONFIG(TAG, "  Speaker: no");
#endif
  ESP_LOGCONFIG(TAG, "  Media direction: %s", this->media_direction_());
}

void SipClient::on_shutdown() {
  if (this->transport_) this->transport_->close();
}

void SipClient::send_raw_(const std::string &msg) {
  if (!this->transport_ || !this->transport_->connected()) return;
  if (!this->transport_->send(msg)) {
    ESP_LOGW(TAG, "SIP send failed (%u bytes)", static_cast<unsigned>(msg.size()));
    return;
  }
  ESP_LOGV(TAG, "TX >>>\n%s", msg.c_str());
}

// Scan configured preference order against the remote SDP's available PTs.
static ChosenAudio choose_payload_(const SdpInfo &sdp, const std::vector<AudioCodecId> &prefs) {
  return choose_audio_codec(sdp, prefs);
}

void SipClient::set_state_(SipState s) {
  if (this->state_ != s) {
    ESP_LOGD(TAG, "state %d -> %d", this->state_, s);
    this->state_ = s;
  }
}

// ---------------- registration ----------------

void SipClient::do_register_() {
  this->reg_call_id_ = gen_call_id(this->transport_->local_ip());
  this->reg_tag_ = gen_tag();
  this->reg_branch_ = gen_branch();
  this->reg_cseq_++;
  this->register_auth_tried_ = false;
  this->send_raw_(this->build_register_());
  this->set_state_(SIP_REGISTERING);
  this->next_register_ms_ = millis() + 5000;  // retry window if no response
}

std::string SipClient::contact_uri_() {
  return sip_contact_uri(this->username_, this->transport_->contact_host_port(),
                         this->transport_->contact_transport_param());
}

std::string SipClient::via_header_(const std::string &branch) {
  return sip_via_header(this->transport_->via_token(), this->transport_->via_sent_by(), branch);
}

std::string SipClient::build_register_() {
  std::string aor = "sip:" + this->username_ + "@" + this->domain_;
  std::string reg_uri = "sip:" + this->domain_;
  std::string disp = this->caller_id_.empty() ? this->username_ : this->caller_id_;
  std::string msg;
  msg += "REGISTER " + reg_uri + " SIP/2.0\r\n";
  msg += this->via_header_(this->reg_branch_);
  msg += "Max-Forwards: 70\r\n";
  msg += "From: \"" + disp + "\" <" + aor + ">;tag=" + this->reg_tag_ + "\r\n";
  msg += "To: <" + aor + ">\r\n";
  msg += "Call-ID: " + this->reg_call_id_ + "\r\n";
  msg += "CSeq: " + std::to_string(this->reg_cseq_) + " REGISTER\r\n";
  msg += "Contact: " + this->contact_uri_() + "\r\n";
  msg += "Expires: " + std::to_string(this->expiration_) + "\r\n";
  msg += "User-Agent: " + std::string(USER_AGENT) + "\r\n";
  msg += "Content-Length: 0\r\n\r\n";
  return msg;
}

void SipClient::handle_register_response_(const SipMessage &m) {
  std::string cseq_str = m.header("CSeq");
  uint32_t cseq_num = (uint32_t) std::atoi(cseq_str.c_str());
  if (cseq_num != this->reg_cseq_) {
    ESP_LOGD(TAG, "Ignoring REGISTER response for old CSeq %u", static_cast<unsigned>(cseq_num));
    return;
  }

  if ((m.status_code == 401 || m.status_code == 407) && !this->register_auth_tried_) {
    this->register_auth_tried_ = true;
    bool proxy = m.status_code == 407;
    std::string ch = m.header(proxy ? "Proxy-Authenticate" : "WWW-Authenticate");
    std::string realm = auth_param(ch, "realm");
    std::string nonce = auth_param(ch, "nonce");
    std::string qop = auth_param(ch, "qop");
    std::string opaque = auth_param(ch, "opaque");
    std::string uri = "sip:" + this->domain_;
    std::string nc = "00000001";
    std::string cnonce = gen_random_hex(8);
    const std::string &auth_user = this->auth_user_();
    std::string resp = digest_response(auth_user, this->password_, realm, "REGISTER", uri,
                                       nonce, qop.empty() ? "" : "auth", nc, cnonce);

    this->reg_cseq_++;
    this->reg_branch_ = gen_branch();
    std::string msg = this->build_register_();
    // insert Authorization before Content-Length
    std::string auth = std::string(proxy ? "Proxy-Authorization: " : "Authorization: ") +
                       "Digest username=\"" + auth_user + "\", realm=\"" + realm +
                       "\", nonce=\"" + nonce + "\", uri=\"" + uri + "\", response=\"" + resp +
                       "\", algorithm=MD5";
    if (!qop.empty()) auth += ", qop=auth, nc=" + nc + ", cnonce=\"" + cnonce + "\"";
    if (!opaque.empty()) auth += ", opaque=\"" + opaque + "\"";
    auth += "\r\n";
    size_t pos = msg.find("Content-Length:");
    msg.insert(pos, auth);
    this->send_raw_(msg);
    return;
  }
  if (m.status_code >= 200 && m.status_code < 300) {
    bool was = this->registered_;
    this->registered_ = true;
    this->set_state_(SIP_REGISTERED);
    this->next_register_ms_ = millis() + (this->expiration_ * 1000) / 2;
    if (!was) {
      ESP_LOGI(TAG, "Registered with %s", this->server_.c_str());
      this->registered_cb_.call();
    }
    return;
  }
  ESP_LOGW(TAG, "REGISTER failed: %d %s", m.status_code, m.reason.c_str());
  this->next_register_ms_ = millis() + 10000;  // retry later
}

// ---------------- outbound call ----------------

void SipClient::call(const std::string &number) {
  if (this->state_ != SIP_REGISTERED) {
    ESP_LOGW(TAG, "Cannot call in state %d", this->state_);
    return;
  }
  this->invite_number_ = number;
  this->outbound_ = true;
  this->invite_auth_tried_ = false;
  this->d_call_id_ = gen_call_id(this->transport_->local_ip());
  this->d_local_tag_ = gen_tag();
  this->d_branch_ = gen_branch();
  this->d_cseq_ = 1;
  this->reset_call_session_();
  std::string disp = this->caller_id_.empty() ? this->username_ : this->caller_id_;
  this->d_local_ = "\"" + disp + "\" <sip:" + this->username_ + "@" + this->domain_ +
                   ">;tag=" + this->d_local_tag_;
  this->d_remote_ = "<sip:" + number + "@" + this->domain_ + ">";
  this->d_remote_target_ = "sip:" + number + "@" + this->domain_;
  this->d_invite_uri_ = this->d_remote_target_;
  this->dialog_routes_.clear();
  this->send_raw_(this->build_invite_());
  this->set_state_(SIP_INVITING);
  ESP_LOGI(TAG, "Calling %s", number.c_str());
}

void SipClient::apply_chosen_codec_(int pt, AudioCodecId id) {
  // Defense in depth: never free the codec object while rtp_ holds a raw ptr.
  if (this->media_active_) {
    ESP_LOGW(TAG, "Ignoring codec change while media is active");
    return;
  }
  this->chosen_pt_ = pt;
  switch (id) {
    case AudioCodecId::G722:
      this->active_codec_ = make_g722_codec((uint8_t) pt);
      break;
    default:
      this->active_codec_ = make_g711_codec((uint8_t) pt, id);
      break;
  }
  this->chosen_rtpmap_ = this->active_codec_->desc().rtpmap;
}

std::string SipClient::local_sdp_(bool answer, const char *direction) {
  SdpMediaParams p;
  // RFC 3264 §8: the o= session id stays the same for the whole call and the
  // version grows whenever the SDP we send changes (hold, codec, ...).
  if (this->sdp_session_id_.empty()) {
    const uint32_t now = millis();
    this->sdp_session_id_ = std::to_string(now);
    this->sdp_version_ = now;
  }
  p.session_id = this->sdp_session_id_;
  p.local_ip = this->transport_->local_ip();
  p.rtp_port = this->local_rtp_port_;
  p.direction = direction != nullptr ? direction : direction_name(this->local_dir_);
  p.answer = answer;
  if (answer) {
    p.answer_pt = this->chosen_pt_ >= 0 ? this->chosen_pt_ : 0;
    p.answer_rtpmap = this->chosen_rtpmap_ != nullptr ? this->chosen_rtpmap_ : "PCMU/8000";
    p.dtmf_pt = this->remote_dtmf_pt_;
  } else {
    p.dtmf_pt = 101;
    p.offer_codecs = this->configured_codecs_;
  }
  // Compare the body without its version to see whether anything changed.
  p.session_version = "0";
  const std::string signature = build_sdp_body(p);
  if (!this->sdp_signature_.empty() && signature != this->sdp_signature_) this->sdp_version_++;
  this->sdp_signature_ = signature;
  p.session_version = std::to_string(this->sdp_version_);
  return build_sdp_body(p);
}

std::string SipClient::build_invite_() {
  std::string sdp = this->local_sdp_();
  std::string msg;
  msg += "INVITE " + this->d_invite_uri_ + " SIP/2.0\r\n";
  msg += this->via_header_(this->d_branch_);
  msg += "Max-Forwards: 70\r\n";
  msg += "From: " + this->d_local_ + "\r\n";
  msg += "To: " + this->d_remote_ + "\r\n";
  msg += "Call-ID: " + this->d_call_id_ + "\r\n";
  msg += "CSeq: " + std::to_string(this->d_cseq_) + " INVITE\r\n";
  msg += "Contact: " + this->contact_uri_() + "\r\n";
  msg += "Allow: " + std::string(ALLOW_METHODS) + "\r\n";
  msg += "User-Agent: " + std::string(USER_AGENT) + "\r\n";
  msg += "Content-Type: application/sdp\r\n";
  msg += "Content-Length: " + std::to_string(sdp.size()) + "\r\n\r\n";
  msg += sdp;
  return msg;
}

std::string SipClient::build_ack_(const SipMessage &resp) {
  std::string to = resp.header("To");
  bool success = resp.status_code >= 200 && resp.status_code < 300;

  // RFC 3261 §17.1.1.3: the ACK for a 3xx-6xx belongs to the INVITE
  // transaction — same Request-URI and same top Via branch — or the server
  // keeps retransmitting the failure (seen with 3CX after a 407). The ACK
  // for a 2xx is its own transaction: new branch, sent to that response's
  // Contact through that response's route set — not the stored dialog's,
  // so a forked 2xx from another leg is acknowledged along its own path.
  std::string target;
  std::string route_block;
  std::string branch;
  if (success) {
    target = extract_angle_uri(resp.header("Contact"));
    if (target.empty()) target = this->d_remote_target_;
    std::vector<std::string> routes = split_header_values(resp.header("Record-Route"));
    std::reverse(routes.begin(), routes.end());
    apply_route_set(routes, target, route_block);
    branch = gen_branch();
  } else {
    target = this->d_invite_uri_.empty() ? this->d_remote_target_ : this->d_invite_uri_;
    // Take the branch from the response itself so a late retransmission of
    // an old transaction (e.g. the 407 while the authenticated INVITE is
    // already in flight) is acknowledged with its own branch.
    branch = via_branch(resp.header("Via"));
    if (branch.empty()) branch = this->d_branch_;
  }

  uint32_t cseq = (uint32_t) std::atoi(resp.header("CSeq").c_str());
  if (cseq == 0) {
    cseq = this->d_cseq_;
  }

  std::string msg;
  msg += "ACK " + target + " SIP/2.0\r\n";
  msg += this->via_header_(branch);
  msg += "Max-Forwards: 70\r\n";
  msg += route_block;
  msg += "From: " + this->d_local_ + "\r\n";
  msg += "To: " + (to.empty() ? this->d_remote_ : to) + "\r\n";
  msg += "Call-ID: " + this->d_call_id_ + "\r\n";
  msg += "CSeq: " + std::to_string(cseq) + " ACK\r\n";
  msg += "Content-Length: 0\r\n\r\n";
  return msg;
}

void SipClient::handle_invite_response_(const SipMessage &m, const std::string &raw) {
  if (!this->outbound_) return;

  uint32_t cseq_num = (uint32_t) std::atoi(m.header("CSeq").c_str());
  if (cseq_num != this->d_cseq_) {
    // Ignore responses for old transactions, but re-ACK final failures (>=300)
    // to stop server retransmissions.
    if (m.status_code >= 300 && m.status_code < 700) {
      this->send_raw_(this->build_ack_(m));
    }
    return;
  }

  if ((m.status_code == 401 || m.status_code == 407) && !this->invite_auth_tried_) {
    // ACK the failure response first.
    this->send_raw_(this->build_ack_(m));
    this->invite_auth_tried_ = true;
    bool proxy = m.status_code == 407;
    std::string ch = m.header(proxy ? "Proxy-Authenticate" : "WWW-Authenticate");
    std::string realm = auth_param(ch, "realm");
    std::string nonce = auth_param(ch, "nonce");
    std::string qop = auth_param(ch, "qop");
    std::string opaque = auth_param(ch, "opaque");
    std::string uri = this->d_remote_target_;
    std::string nc = "00000001";
    std::string cnonce = gen_random_hex(8);
    const std::string &auth_user = this->auth_user_();
    std::string resp = digest_response(auth_user, this->password_, realm, "INVITE", uri,
                                       nonce, qop.empty() ? "" : "auth", nc, cnonce);
    this->d_cseq_++;
    this->d_branch_ = gen_branch();
    std::string msg = this->build_invite_();
    std::string auth = std::string(proxy ? "Proxy-Authorization: " : "Authorization: ") +
                       "Digest username=\"" + auth_user + "\", realm=\"" + realm +
                       "\", nonce=\"" + nonce + "\", uri=\"" + uri + "\", response=\"" + resp +
                       "\", algorithm=MD5";
    if (!qop.empty()) auth += ", qop=auth, nc=" + nc + ", cnonce=\"" + cnonce + "\"";
    if (!opaque.empty()) auth += ", opaque=\"" + opaque + "\"";
    auth += "\r\n";
    size_t pos = msg.find("Content-Type:");
    msg.insert(pos, auth);
    this->send_raw_(msg);
    return;
  }

  if (m.status_code >= 100 && m.status_code < 200) {
    if (m.status_code == 180 || m.status_code == 183) this->set_state_(SIP_RINGING_OUT);
    return;
  }

  if (m.status_code >= 200 && m.status_code < 300) {
    // Retransmitted INVITE 2xx (ACK lost): re-ACK only. Re-running this block
    // would replace active_codec_ while rtp_ still holds the old raw pointer.
    if (this->state_ != SIP_INVITING && this->state_ != SIP_RINGING_OUT) {
      this->send_raw_(this->build_ack_(m));
      return;
    }

    // Capture remote tag, target and route set, parse SDP, ACK, start media.
    std::string to = m.header("To");
    if (!to.empty()) this->d_remote_ = to;
    std::string contact = m.header("Contact");
    std::string target = extract_angle_uri(contact);
    if (!target.empty())
      this->d_remote_target_ = target;
    // RFC 3261 §12.1.2: the UAC's route set is the Record-Route list of the
    // 2xx in reverse order.
    this->dialog_routes_ = split_header_values(m.header("Record-Route"));
    std::reverse(this->dialog_routes_.begin(), this->dialog_routes_.end());

    SdpInfo sdp = parse_sdp(m.body);
    // A hold address (c=0.0.0.0) is not an RTP destination; media starts once a
    // real endpoint arrives in a re-INVITE.
    if (sdp.on_hold)
      this->remote_rtp_ip_.clear();
    else if (!sdp.connection_ip.empty())
      this->remote_rtp_ip_ = sdp.connection_ip;
    this->remote_rtp_port_ = sdp.audio_port;
    ChosenAudio chosen = choose_payload_(sdp, this->configured_codecs_);
    this->remote_dtmf_pt_ = sdp.telephone_event_pt;
    if (chosen.pt < 0) {
      // Intentional: empty / non-G.711 answer SDP used to fall back to PT 0 and
      // leave a silent "connected" call. Reject instead. Dialog is confirmed by
      // ACK, so send BYE so the PBX does not wait for rtp_timeout.
      this->send_raw_(this->build_ack_(m));
      this->d_cseq_++;
      this->send_raw_(this->build_request_in_dialog_("BYE"));
      ESP_LOGW(TAG, "Call failed: no common audio codec in answer SDP");
      this->stop_media_();
      this->reset_call_session_();
      this->set_state_(SIP_REGISTERED);
      this->ended_cb_.call();
      return;
    }
    this->apply_chosen_codec_(chosen.pt, chosen.id);
    this->apply_initial_direction_(sdp);

    this->send_raw_(this->build_ack_(m));
    this->start_media_();
    this->set_state_(SIP_IN_CALL);
    // RFC 4028: we do not offer Supported: timer, so a compliant UAS leaves the
    // header out. If the peer still negotiated one, only refresher=uas (the
    // peer) is something we can live with.
    const SessionExpires se = parse_session_expires(m.header("Session-Expires"));
    if (peer_refreshes_in_response(se)) {
      this->session_timer_active_ = true;
      this->session_interval_s_ = se.delta;
      this->arm_session_watchdog_();
    } else if (se.valid && se.refresher == Refresher::UAC) {
      ESP_LOGW(TAG, "Peer put us in charge of the %us session refresh, which this client never does; "
                    "the PBX may end the call at expiry",
               static_cast<unsigned>(se.delta));
    }
    ESP_LOGI(TAG, "Call connected");
    this->connected_cb_.call();
    return;
  }

  // >= 300 final failure: ACK and end.
  this->send_raw_(this->build_ack_(m));
  ESP_LOGW(TAG, "Call failed: %d %s", m.status_code, m.reason.c_str());
  this->stop_media_();
  this->reset_call_session_();
  this->set_state_(SIP_REGISTERED);
  this->ended_cb_.call();
}

// ---------------- inbound requests ----------------

std::string SipClient::extract_caller_(const SipMessage &m) {
  std::string from = m.header("From");
  size_t lt = from.find("sip:");
  if (lt == std::string::npos) return from;
  size_t at = from.find('@', lt);
  size_t gt = from.find_first_of(">;", lt);
  size_t end = (at != std::string::npos && (gt == std::string::npos || at < gt)) ? at : gt;
  if (end == std::string::npos) end = from.size();
  return from.substr(lt + 4, end - (lt + 4));
}

std::string SipClient::build_response_(const SipMessage &req, int code, const std::string &reason,
                                       bool with_sdp, const std::string &extra_headers,
                                       const std::string &sdp_body) {
  std::string to = req.header("To");
  if (to.find("tag=") == std::string::npos) to += ";tag=" + this->d_local_tag_;
  // with_sdp: the 200 OK answer SDP of an INVITE / UPDATE (sdp_body overrides
  // the default one, e.g. the hold direction of a re-INVITE).
  std::string sdp = with_sdp ? (sdp_body.empty() ? this->local_sdp_(/*answer=*/true) : sdp_body) : "";
  const bool success = code >= 200 && code < 300;
  std::string msg;
  msg += "SIP/2.0 " + std::to_string(code) + " " + reason + "\r\n";
  // Echo every Via, topmost first: a request that crossed a proxy/SBC has
  // one per hop and the response must retrace them all (RFC 3261 §8.2.6.2).
  for (const auto &via : split_header_values(req.header("Via")))
    msg += "Via: " + via + "\r\n";
  msg += "From: " + req.header("From") + "\r\n";
  msg += "To: " + to + "\r\n";
  msg += "Call-ID: " + req.header("Call-ID") + "\r\n";
  msg += "CSeq: " + req.header("CSeq") + "\r\n";
  // RFC 3261 §12.1.1: every dialog-establishing response — the early dialog
  // of a 18x included — must echo the request's Record-Route and carry a
  // Contact the peer can route in-dialog requests to. Dropping either
  // strands the proxy outside the dialog and its ACK never reaches us.
  // (For a re-INVITE the route set is fixed already; echoing it is harmless.)
  if (req.method == "INVITE" && code > 100 && code < 300) {
    for (const auto &route : split_header_values(req.header("Record-Route")))
      msg += "Record-Route: " + route + "\r\n";
    msg += "Contact: " + this->contact_uri_() + "\r\n";
  } else if (req.method == "UPDATE" && success) {
    // RFC 3311 §5.2: a 2xx to UPDATE carries our Contact too (target refresh).
    msg += "Contact: " + this->contact_uri_() + "\r\n";
  }
  // RFC 3261 §20.5: Allow in 2xx to INVITE / OPTIONS (UPDATE: RFC 3311 §5.2).
  if (success && (req.method == "INVITE" || req.method == "UPDATE" || req.method == "OPTIONS"))
    msg += "Allow: " + std::string(ALLOW_METHODS) + "\r\n";
  msg += extra_headers;
  msg += "User-Agent: " + std::string(USER_AGENT) + "\r\n";
  if (with_sdp) {
    msg += "Content-Type: application/sdp\r\n";
    msg += "Content-Length: " + std::to_string(sdp.size()) + "\r\n\r\n";
    msg += sdp;
  } else {
    msg += "Content-Length: 0\r\n\r\n";
  }
  return msg;
}

void SipClient::handle_request_(const SipMessage &m, const std::string &raw) {
  const std::string &method = m.method;

  if (method == "INVITE" || method == "UPDATE") {
    // A To tag means the request claims to be inside an existing dialog
    // (re-INVITE / UPDATE, RFC 3261 §12.2.2); only a tag-less INVITE starts a
    // new call.
    const DialogVerdict verdict = classify_dialog_request(this->dialog_state_(), m);
    if (verdict != DialogVerdict::NEW_DIALOG) {
      this->handle_in_dialog_request_(m, verdict);
      return;
    }
    if (method == "UPDATE") {
      // UPDATE is only defined inside a dialog (RFC 3311 §5.2).
      this->send_raw_(this->build_response_(m, 481, "Call/Transaction Does Not Exist", false));
      return;
    }
  }

  if (method == "INVITE") {
    if (this->state_ != SIP_REGISTERED) {
      this->send_raw_(this->build_response_(m, 486, "Busy Here", false));
      return;
    }
    // Negotiate codec before touching dialog state so a 488 does not leave
    // stale Call-ID / RTP endpoint fields for the next call.
    SdpInfo sdp = parse_sdp(m.body);
    ChosenAudio chosen = choose_payload_(sdp, this->configured_codecs_);
    if (chosen.pt < 0) {
      // To-tag for the 488 only. Leave it set — clearing would make a later
      // OPTIONS 200 OK emit a malformed empty ";tag=" via build_response_.
      this->d_local_tag_ = gen_tag();
      this->send_raw_(this->build_response_(m, 488, "Not Acceptable Here", false));
      ESP_LOGW(TAG, "Incoming INVITE rejected: no common audio codec");
      return;
    }
    // RFC 4028 §6: refuse a session interval below our minimum before the
    // call exists; the caller retries with a larger one.
    const SessionTimerDecision timer = decide_session_timer(
        m.header("Session-Expires"), m.header("Supported"), m.header("Require"));
    if (timer.action == SessionTimerDecision::Action::REJECT) {
      this->d_local_tag_ = gen_tag();
      this->send_raw_(this->build_response_(m, 422, "Session Interval Too Small", false,
                                            "Min-SE: " + std::to_string(timer.min_se) + "\r\n"));
      ESP_LOGW(TAG, "Incoming INVITE rejected: Session-Expires below %us",
               static_cast<unsigned>(timer.min_se));
      return;
    }

    // New inbound dialog.
    this->reset_call_session_();
    this->outbound_ = false;
    this->incoming_invite_ = m;
    this->incoming_invite_raw_ = raw;
    this->d_call_id_ = m.header("Call-ID");
    this->d_local_tag_ = gen_tag();
    this->d_local_ = m.header("To");  // we are the To; tag added in responses
    if (this->d_local_.find("tag=") == std::string::npos)
      this->d_local_ += ";tag=" + this->d_local_tag_;
    this->d_remote_ = m.header("From");
    std::string contact = m.header("Contact");
    this->d_remote_target_ = extract_angle_uri(contact);
    this->d_invite_uri_.clear();
    // RFC 3261 §12.1.1: the UAS's route set is the Record-Route list as-is.
    this->dialog_routes_ = split_header_values(m.header("Record-Route"));
    // Our own CSeq space for BYE starts above the INVITE's (historic
    // behaviour, accepted by every PBX tried so far); the peer's CSeq is
    // tracked separately for in-dialog ordering checks.
    this->d_cseq_ = std::atoi(m.header("CSeq").c_str());
    this->d_remote_cseq_ = this->d_cseq_;
    this->d_initial_cseq_ = this->d_cseq_;
    this->session_resp_headers_.clear();
    this->commit_session_timer_(timer);
    // A hold address (c=0.0.0.0) is not an RTP destination (RFC 3264 §8.4).
    this->remote_rtp_ip_ = sdp.on_hold ? std::string() : sdp.connection_ip;
    this->remote_rtp_port_ = sdp.audio_port;
    this->apply_chosen_codec_(chosen.pt, chosen.id);
    this->apply_initial_direction_(sdp);
    this->remote_dtmf_pt_ = sdp.telephone_event_pt;

    this->send_raw_(this->build_response_(m, 100, "Trying", false));
    this->send_raw_(this->build_response_(m, 180, "Ringing", false));
    this->set_state_(SIP_INCOMING);
    std::string caller = this->extract_caller_(m);
    ESP_LOGI(TAG, "Incoming call from %s", caller.c_str());
    this->incoming_call_cb_.call(caller);
    return;
  }

  if (method == "ACK") {
    this->handle_ack_(m);
    return;
  }

  if (method == "BYE") {
    this->send_raw_(this->build_response_(m, 200, "OK", false));
    ESP_LOGI(TAG, "Remote hung up");
    this->stop_media_();
    this->reset_call_session_();
    this->set_state_(SIP_REGISTERED);
    this->ended_cb_.call();
    return;
  }

  if (method == "CANCEL") {
    this->send_raw_(this->build_response_(m, 200, "OK", false));
    if (this->state_ == SIP_INCOMING) {
      this->send_raw_(this->build_response_(this->incoming_invite_, 487, "Request Terminated", false));
      this->reset_call_session_();
      this->set_state_(SIP_REGISTERED);
      this->ended_cb_.call();
    }
    return;
  }

  if (method == "OPTIONS") {
    this->send_raw_(this->build_response_(m, 200, "OK", false));
    return;
  }

  if (method == "INFO") {
    this->send_raw_(this->build_response_(m, 200, "OK", false));
    char digit = parse_dtmf_info(m.header("Content-Type"), m.body);
    if (digit == 0) return;
    if (this->state_ != SIP_IN_CALL) {
      ESP_LOGW(TAG, "DTMF INFO '%c' ignored in state %d", digit, this->state_);
      return;
    }
    ESP_LOGI(TAG, "DTMF via SIP INFO: %c", digit);
    this->dtmf_cb_.call(std::string(1, digit));
    return;
  }

  // Unknown in-dialog request: acknowledge.
  this->send_raw_(this->build_response_(m, 200, "OK", false));
}

// ---------------- in-dialog requests (re-INVITE / UPDATE) ----------------

DialogState SipClient::dialog_state_() const {
  DialogState st;
  // A dialog only exists while a call is being set up or running; the ids of a
  // finished call stay in the members for late retransmissions of failure
  // responses and must not match new requests.
  const bool established =
      this->state_ == SIP_INCOMING || this->state_ == SIP_ANSWERING || this->state_ == SIP_IN_CALL;
  const bool early_outbound = this->state_ == SIP_INVITING || this->state_ == SIP_RINGING_OUT;
  if (established || early_outbound) {
    st.call_id = this->d_call_id_;
    st.local_tag = this->d_local_tag_;
    st.remote_tag = header_tag(this->d_remote_);
    // The peer's tag of an outbound call is only stored from the 2xx; before
    // that an in-dialog request (UPDATE) matches on Call-ID + our tag.
    st.remote_tag_unknown = early_outbound;
  }
  st.remote_cseq = this->d_remote_cseq_;
  st.last_response_cseq = this->last_response_cseq_;
  st.last_response_method = this->last_response_method_;
  return st;
}

void SipClient::send_final_response_(const SipMessage &m, uint32_t cseq, const std::string &resp) {
  // Kept verbatim: a retransmitted request (UDP, our response was lost) is
  // answered with the same bytes instead of being processed twice.
  this->last_response_ = resp;
  this->last_response_cseq_ = cseq;
  this->last_response_method_ = m.method;
  this->d_remote_cseq_ = cseq;
  this->send_raw_(resp);
}

// RFC 3261 §14.2: an offer while another one is still unfinished is told to
// retry after a random 0-10 s. Stored like any final response so a
// retransmission of the same request is not mistaken for a new transaction.
void SipClient::send_retry_later_(const SipMessage &m, uint32_t cseq) {
  const std::string retry = "Retry-After: " + std::to_string(std::rand() % 11) + "\r\n";
  this->send_final_response_(m, cseq,
                             this->build_response_(m, 500, "Server Internal Error", false, retry));
}

// Our 200 to an offer-less re-INVITE carries an offer; its answer rides in the
// ACK. Give up waiting after 64*T1 (32 s): UDP 2xx are not retransmitted here.
bool SipClient::reinvite_offer_pending_() const {
  return this->reinvite_ack_pending_ && this->reinvite_offerless_ &&
         static_cast<int32_t>(millis() - this->reinvite_ack_ms_) < 0;
}

std::string SipClient::commit_session_timer_(const SessionTimerDecision &d) {
  std::string headers;
  if (d.action == SessionTimerDecision::Action::ACCEPT) {
    this->session_timer_active_ = true;
    this->session_interval_s_ = d.interval;
    headers = session_timer_headers(d.interval, d.require_timer);
  } else {
    // No (usable) Session-Expires: the peer does not use the timer for this
    // session (RFC 4028 §9), so there is no refresh to wait for.
    if (d.ends_timer) ESP_LOGI(TAG, "Session timer ended by the peer");
    this->session_timer_active_ = false;
    this->session_armed_ = false;
  }
  if (this->state_ == SIP_IN_CALL) this->arm_session_watchdog_();
  this->session_resp_headers_ = headers;
  return headers;
}

void SipClient::arm_session_watchdog_() {
  if (!this->session_timer_active_) return;
  this->session_armed_ = true;
  this->session_deadline_ms_ = millis() + session_watchdog_ms(this->session_interval_s_);
  ESP_LOGD(TAG, "Session timer: peer refreshes every %us, giving up after %ums",
           static_cast<unsigned>(this->session_interval_s_),
           static_cast<unsigned>(session_watchdog_ms(this->session_interval_s_)));
}

void SipClient::reset_call_session_() {
  this->d_remote_cseq_ = 0;
  this->d_initial_cseq_ = 0;
  this->last_response_.clear();
  this->last_response_cseq_ = 0;
  this->last_response_method_.clear();
  this->reinvite_ack_pending_ = false;
  this->reinvite_ack_cseq_ = 0;
  this->reinvite_offerless_ = false;
  this->reinvite_ack_ms_ = 0;
  this->local_dir_ = this->capability_direction_();
  this->tx_allowed_ = true;
  this->rx_allowed_ = true;
  this->rtp_.set_send_enabled(true);
  this->rtp_.set_receive_enabled(true);
  this->sdp_session_id_.clear();
  this->sdp_version_ = 0;
  this->sdp_signature_.clear();
  this->session_timer_active_ = false;
  this->session_armed_ = false;
  this->session_interval_s_ = 0;
  this->session_deadline_ms_ = 0;
  this->session_resp_headers_.clear();
}

// RFC 3264 §6.1: the answer's direction must answer the offer's (never
// sendrecv to a sendonly / inactive offer). Used for the initial SDP of an
// inbound call and for the answer to our own INVITE.
void SipClient::apply_initial_direction_(const SdpInfo &sdp) {
  this->local_dir_ = answer_direction(sdp.direction, sdp.on_hold, this->capability_direction_());
  const SdpDirection wire = answer_direction(sdp.direction, sdp.on_hold, SdpDirection::SENDRECV);
  this->tx_allowed_ = direction_sends(wire);
  this->rx_allowed_ = direction_receives(wire);
}

bool SipClient::apply_remote_sdp_(const SdpInfo &sdp, SdpDirection offered) {
  if (!sdp_has_usable_audio(sdp)) return false;
  // Keep the codec in use while it is still offered: a re-anchor or transfer
  // must not restart the audio just because the preference order differs.
  const ChosenAudio chosen = choose_audio_codec_keeping(
      sdp, this->configured_codecs_, this->chosen_pt_,
      this->active_codec_ != nullptr ? static_cast<int>(this->active_codec_->id()) : -1);
  if (chosen.pt < 0) return false;

  NegotiatedMedia cur;
  cur.pt = this->chosen_pt_;
  cur.codec_id = this->active_codec_ != nullptr ? static_cast<int>(this->active_codec_->id()) : -1;
  cur.ip = this->remote_rtp_ip_;
  cur.port = this->remote_rtp_port_;
  cur.dtmf_pt = this->remote_dtmf_pt_;
  NegotiatedMedia next = cur;
  next.pt = chosen.pt;
  next.codec_id = static_cast<int>(chosen.id);
  next.dtmf_pt = sdp.telephone_event_pt;
  // c=0.0.0.0 is hold, not an address: never hand it to the RTP socket
  // (RFC 3264 §8.4); keep sending to the old endpoint for the unhold.
  if (!sdp.on_hold && !sdp.connection_ip.empty()) {
    next.ip = sdp.connection_ip;
    next.port = sdp.audio_port;
  }
  const MediaDelta delta = diff_media(cur, next);

  // What we advertise follows our device capability; whether RTP flows only
  // follows what the peer asks for. A speaker-only device answers "recvonly"
  // yet keeps sending idle silence (NAT / symmetric-RTP latching) until the
  // peer actually stops taking media (sendonly / inactive / 0.0.0.0).
  this->local_dir_ = answer_direction(sdp.direction, sdp.on_hold, offered);
  const SdpDirection wire = answer_direction(sdp.direction, sdp.on_hold, SdpDirection::SENDRECV);
  this->tx_allowed_ = direction_sends(wire);
  this->rx_allowed_ = direction_receives(wire);

  if (delta.restart_needed()) {
    // apply_chosen_codec_() refuses while media runs and rtp_ holds a raw
    // pointer to the codec: stop, swap, start (a brief audio gap).
    const bool was_active = this->media_active_;
    const bool was_talking = this->talking_;  // half-duplex push-to-talk
    this->stop_media_();
    this->remote_rtp_ip_ = next.ip;
    this->remote_rtp_port_ = next.port;
    this->remote_dtmf_pt_ = next.dtmf_pt;
    this->apply_chosen_codec_(chosen.pt, chosen.id);
    ESP_LOGI(TAG, "Re-INVITE: restarting media (%s, remote %s:%u)",
             this->chosen_rtpmap_ != nullptr ? this->chosen_rtpmap_ : "?", this->remote_rtp_ip_.c_str(),
             static_cast<unsigned>(this->remote_rtp_port_));
    if (was_active || this->state_ == SIP_IN_CALL) this->start_media_();
    // start_media_() comes up listening; put the bus back to the microphone.
    if (was_talking) this->start_talking();
  } else {
    this->remote_rtp_ip_ = next.ip;
    this->remote_rtp_port_ = next.port;
    this->remote_dtmf_pt_ = next.dtmf_pt;
    if (this->media_active_) {
      if (delta.remote_changed) {
        this->rtp_.set_remote(this->remote_rtp_ip_, this->remote_rtp_port_);
        ESP_LOGI(TAG, "Re-INVITE: remote RTP endpoint now %s:%u", this->remote_rtp_ip_.c_str(),
                 static_cast<unsigned>(this->remote_rtp_port_));
      }
      if (delta.dtmf_changed) this->rtp_.set_dtmf_payload_type(this->remote_dtmf_pt_);
      this->rtp_.set_send_enabled(this->tx_allowed_);
      this->rtp_.set_receive_enabled(this->rx_allowed_);
    } else if (this->state_ == SIP_IN_CALL) {
      // The call came up without a usable RTP endpoint (e.g. it started on
      // hold); start the media now that one has arrived.
      this->start_media_();
    }
  }
  ESP_LOGI(TAG, "Media direction now %s (send %s, receive %s)", direction_name(this->local_dir_),
           this->tx_allowed_ ? "on" : "held", this->rx_allowed_ ? "on" : "held");
  return true;
}

void SipClient::handle_in_dialog_request_(const SipMessage &m, DialogVerdict verdict) {
  const bool is_update = m.method == "UPDATE";
  switch (verdict) {
    case DialogVerdict::NOT_IN_DIALOG:
      // RFC 3261 §12.2.2: a To tag that names no dialog of ours.
      ESP_LOGW(TAG, "%s for an unknown dialog rejected", m.method.c_str());
      this->send_raw_(this->build_response_(m, 481, "Call/Transaction Does Not Exist", false));
      return;
    case DialogVerdict::RETRANSMISSION:
      ESP_LOGD(TAG, "Retransmitted %s, resending the last response", m.method.c_str());
      this->send_raw_(this->last_response_);
      return;
    case DialogVerdict::STALE_CSEQ:
      ESP_LOGW(TAG, "%s with an out-of-order CSeq rejected", m.method.c_str());
      this->send_raw_(this->build_response_(m, 500, "Server Internal Error", false));
      return;
    default:
      break;
  }

  uint32_t cseq = 0;
  std::string cseq_method;
  parse_cseq(m.header("CSeq"), cseq, cseq_method);  // classify_dialog_request() accepted it
  const bool has_offer = !m.body.empty();

  // RFC 3261 §14.2 / RFC 3311 §5.2: an offer can only be handled once the
  // initial answer is ACKed. A body-less UPDATE is merely a refresh.
  if (this->state_ != SIP_IN_CALL && (!is_update || has_offer)) {
    ESP_LOGW(TAG, "%s while the call is not established, retry later", m.method.c_str());
    this->send_retry_later_(m, cseq);
    return;
  }
  // Body-less UPDATE inside the early dialog of our own outbound INVITE: plain
  // 200, no session timer yet (the call is not up).
  if (this->state_ == SIP_INVITING || this->state_ == SIP_RINGING_OUT) {
    this->send_final_response_(m, cseq, this->build_response_(m, 200, "OK", false));
    return;
  }

  // RFC 3261 §14.1: our 200 made an offer that is still unanswered.
  if (offer_collides(this->reinvite_offer_pending_(), is_update, has_offer)) {
    ESP_LOGW(TAG, "%s while our offer awaits its answer, 491", m.method.c_str());
    this->send_final_response_(m, cseq, this->build_response_(m, 491, "Request Pending", false));
    return;
  }

  // RFC 4028 §6: too short a refresh interval.
  const SessionTimerDecision timer =
      decide_session_timer(m.header("Session-Expires"), m.header("Supported"), m.header("Require"),
                           this->session_timer_active_);
  if (timer.action == SessionTimerDecision::Action::REJECT) {
    ESP_LOGW(TAG, "%s rejected: Session-Expires %us below minimum %us (request Min-SE %us)",
             m.method.c_str(), static_cast<unsigned>(parse_session_expires(m.header("Session-Expires")).delta),
             static_cast<unsigned>(timer.min_se), static_cast<unsigned>(parse_min_se(m.header("Min-SE"))));
    this->send_final_response_(m, cseq,
                               this->build_response_(m, 422, "Session Interval Too Small", false,
                                                     "Min-SE: " + std::to_string(timer.min_se) + "\r\n"));
    return;
  }

  // A new offer supersedes a re-INVITE whose ACK never showed up (a body-less
  // UPDATE is no offer and leaves that state alone).
  if (!is_update || has_offer) {
    this->reinvite_ack_pending_ = false;
    this->reinvite_offerless_ = false;
  }

  if (has_offer) {
    // Validates before touching anything, so a 488 leaves the session as it was
    // (RFC 3261 §14.2).
    if (!this->apply_remote_sdp_(parse_sdp(m.body), this->capability_direction_())) {
      ESP_LOGW(TAG, "%s rejected: no usable audio / common codec in the offer", m.method.c_str());
      this->send_final_response_(m, cseq, this->build_response_(m, 488, "Not Acceptable Here", false));
      return;
    }
  }

  const std::string timer_headers = this->commit_session_timer_(timer);

  if (is_update) {
    // RFC 3311: the answer (if an offer came) rides in the 200; no ACK follows.
    this->send_final_response_(m, cseq, this->build_response_(m, 200, "OK", has_offer, timer_headers));
    ESP_LOGI(TAG, "UPDATE answered%s", has_offer ? " (SDP renegotiated)" : " (session refresh)");
    return;
  }

  // re-INVITE: 200 OK with our SDP, then wait for the ACK. Without an offer
  // (RFC 3261 §14.2, RFC 6337) the 200 carries *our* offer: the negotiated
  // codec only (no codec switch) with our full capability direction, so an
  // offer-less unhold resumes media; the answer comes in the ACK.
  std::string sdp_body;
  if (!has_offer) {
    this->local_dir_ = this->capability_direction_();
    sdp_body = this->local_sdp_(/*answer=*/true, direction_name(this->local_dir_));
  }
  this->reinvite_ack_pending_ = true;
  this->reinvite_ack_cseq_ = cseq;
  this->reinvite_offerless_ = !has_offer;
  this->reinvite_ack_ms_ = millis() + 32000;
  this->send_final_response_(m, cseq,
                             this->build_response_(m, 200, "OK", true, timer_headers, sdp_body));
  ESP_LOGI(TAG, "re-INVITE answered%s", has_offer ? "" : " (offer-less, answer expected in the ACK)");
}

void SipClient::handle_ack_(const SipMessage &m) {
  if (this->state_ == SIP_ANSWERING) {
    // Only the ACK of the INVITE we answered (same Call-ID, same CSeq) completes
    // the call; the ACK of a 486 to another call or of a 500 we sent to an early
    // re-INVITE must not.
    if (!ack_matches(this->d_call_id_, m, this->d_initial_cseq_)) {
      ESP_LOGD(TAG, "Ignoring unrelated ACK");
      return;
    }
    this->set_state_(SIP_IN_CALL);
    this->arm_session_watchdog_();
    ESP_LOGI(TAG, "Call connected (inbound)");
    this->connected_cb_.call();
    return;
  }

  if (this->state_ == SIP_IN_CALL && this->reinvite_ack_pending_ &&
      ack_matches(this->d_call_id_, m, this->reinvite_ack_cseq_)) {
    const bool offerless = this->reinvite_offerless_;
    this->reinvite_ack_pending_ = false;
    this->reinvite_offerless_ = false;
    if (!offerless) return;
    // The answer to the offer our 200 made.
    if (m.body.empty() || !this->apply_remote_sdp_(parse_sdp(m.body), this->capability_direction_())) {
      ESP_LOGW(TAG, "ACK to offer-less re-INVITE carried no usable SDP answer, keeping the old media");
    }
  }
  // Any other ACK is stray (e.g. for a non-2xx we sent) and ignored.
}

void SipClient::answer() {
  if (this->state_ != SIP_INCOMING) {
    ESP_LOGW(TAG, "answer() ignored in state %d", this->state_);
    return;
  }
  this->start_media_();
  this->send_raw_(
      this->build_response_(this->incoming_invite_, 200, "OK", true, this->session_resp_headers_));
  this->set_state_(SIP_ANSWERING);
  ESP_LOGI(TAG, "Answered");
}

void SipClient::hangup() {
  switch (this->state_) {
    case SIP_IN_CALL:
    case SIP_ANSWERING: {
      this->d_cseq_++;
      this->send_raw_(this->build_request_in_dialog_("BYE"));
      this->stop_media_();
      this->reset_call_session_();
      this->set_state_(SIP_REGISTERED);
      this->ended_cb_.call();
      break;
    }
    case SIP_INVITING:
    case SIP_RINGING_OUT: {
      // CANCEL the pending INVITE (same branch/cseq).
      std::string msg;
      msg += "CANCEL " + this->d_invite_uri_ + " SIP/2.0\r\n";
      msg += this->via_header_(this->d_branch_);
      msg += "Max-Forwards: 70\r\n";
      msg += "From: " + this->d_local_ + "\r\n";
      msg += "To: " + this->d_remote_ + "\r\n";
      msg += "Call-ID: " + this->d_call_id_ + "\r\n";
      msg += "CSeq: " + std::to_string(this->d_cseq_) + " CANCEL\r\n";
      msg += "Content-Length: 0\r\n\r\n";
      this->send_raw_(msg);
      this->stop_media_();
      this->reset_call_session_();
      this->set_state_(SIP_REGISTERED);
      this->ended_cb_.call();
      break;
    }
    case SIP_INCOMING: {
      this->send_raw_(this->build_response_(this->incoming_invite_, 603, "Decline", false));
      this->reset_call_session_();
      this->set_state_(SIP_REGISTERED);
      this->ended_cb_.call();
      break;
    }
    default:
      break;
  }
}

std::string SipClient::build_request_in_dialog_(const std::string &method) {
  std::string target = this->d_remote_target_;
  std::string route_block;
  apply_route_set(this->dialog_routes_, target, route_block);
  std::string msg;
  msg += method + " " + target + " SIP/2.0\r\n";
  msg += this->via_header_(gen_branch());
  msg += "Max-Forwards: 70\r\n";
  msg += route_block;
  // For BYE the From/To orientation follows who originates: we are always local.
  msg += "From: " + this->d_local_ + "\r\n";
  msg += "To: " + this->d_remote_ + "\r\n";
  msg += "Call-ID: " + this->d_call_id_ + "\r\n";
  msg += "CSeq: " + std::to_string(this->d_cseq_) + " " + method + "\r\n";
  msg += "User-Agent: " + std::string(USER_AGENT) + "\r\n";
  msg += "Content-Length: 0\r\n\r\n";
  return msg;
}

void SipClient::send_dtmf(const std::string &digits) {
  if (this->state_ != SIP_IN_CALL) {
    ESP_LOGW(TAG, "DTMF ignored: not in call");
    return;
  }
  this->rtp_.queue_dtmf(digits);
}

// ---------------- media ----------------

void SipClient::start_media_() {
  if (this->media_active_) return;
  if (this->remote_rtp_ip_.empty() || this->remote_rtp_port_ == 0) {
    ESP_LOGW(TAG, "No remote RTP endpoint; media not started");
    return;
  }
  if (this->active_codec_ == nullptr) {
    ESP_LOGW(TAG, "No negotiated codec; media not started");
    return;
  }
  this->rtp_.set_codec(this->active_codec_.get());
  this->rtp_.set_dtmf_payload_type(this->remote_dtmf_pt_);
  this->rtp_.set_remote(this->remote_rtp_ip_, this->remote_rtp_port_);
  this->rtp_.set_keepalive_ms(this->rtp_keepalive_ms_);
  // A restart in the middle of a held call must come back up held.
  this->rtp_.set_send_enabled(this->tx_allowed_);
  this->rtp_.set_receive_enabled(this->rx_allowed_);
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    this->rtp_.set_on_audio([this](const int16_t *pcm, size_t n) {
      if (this->half_duplex_ && this->talking_) return;  // speaker is off while transmitting

      // start_speaker_() sets the stream to the codec pcm_rate, so no resample here.
      if (this->channel_ == SIP_CH_STEREO) {
        // Duplicate mono samples to stereo (L/R) for stereo mixers/speakers (e.g. Voice PE).
        std::vector<int16_t> stereo(n * 2);
        for (size_t i = 0; i < n; i++) {
          stereo[i * 2] = pcm[i];
          stereo[i * 2 + 1] = pcm[i];
        }
        this->speaker_->play(reinterpret_cast<const uint8_t *>(stereo.data()), stereo.size() * sizeof(int16_t));
      } else {
        // Mono output (e.g. es8311): push samples as-is.
        this->speaker_->play(reinterpret_cast<const uint8_t *>(pcm), n * sizeof(int16_t));
      }
    });
  } else {
    this->rtp_.set_on_audio({});  // send-only: skip G.711 decode in receive_()
  }
#else
  this->rtp_.set_on_audio({});  // no speaker in build: skip G.711 decode in receive_()
#endif
  this->rtp_.set_on_dtmf([this](char c) {
    std::string s(1, c);
    this->dtmf_cb_.call(s);
  });
  if (!this->rtp_.start(this->local_rtp_port_)) return;

  this->media_active_ = true;
  this->talking_ = false;
  if (this->half_duplex_) {
    // Start in receive mode; the single I2S bus is switched to the mic only
    // while push-to-talk is held (start_talking / stop_talking).
    this->start_speaker_();
  } else {
    this->start_speaker_();
    this->start_mic_();
  }
#ifdef USE_MICROPHONE
  if (this->mic_source_ != nullptr) {
    ESP_LOGI(TAG, "Media started: remote %s:%u pt=%d (%s) dtmf_pt=%d dir=%s (mic %u Hz/%uch/%ubits)%s",
             this->remote_rtp_ip_.c_str(), this->remote_rtp_port_, this->chosen_pt_,
             this->chosen_rtpmap_ != nullptr ? this->chosen_rtpmap_ : "?", this->remote_dtmf_pt_,
             this->media_direction_(), static_cast<unsigned>(this->mic_rate_), this->mic_channels_,
             this->mic_bits_, this->half_duplex_ ? " [half-duplex: listening]" : "");
  } else {
    ESP_LOGI(TAG, "Media started: remote %s:%u pt=%d (%s) dtmf_pt=%d dir=%s",
             this->remote_rtp_ip_.c_str(), this->remote_rtp_port_, this->chosen_pt_,
             this->chosen_rtpmap_ != nullptr ? this->chosen_rtpmap_ : "?", this->remote_dtmf_pt_,
             this->media_direction_());
  }
#else
  ESP_LOGI(TAG, "Media started: remote %s:%u pt=%d (%s) dtmf_pt=%d dir=%s",
           this->remote_rtp_ip_.c_str(), this->remote_rtp_port_, this->chosen_pt_,
           this->chosen_rtpmap_ != nullptr ? this->chosen_rtpmap_ : "?", this->remote_dtmf_pt_,
           this->media_direction_());
#endif
}

void SipClient::stop_media_() {
  if (!this->media_active_) return;
  this->stop_mic_();
  this->stop_speaker_();
  this->rtp_.stop();
  this->media_active_ = false;
  this->talking_ = false;
}

void SipClient::start_speaker_() {
#ifdef USE_SPEAKER
  if (this->speaker_ == nullptr) return;
  const uint32_t rate =
      this->active_codec_ != nullptr ? this->active_codec_->desc().pcm_rate : 8000;
  this->speaker_->set_audio_stream_info(audio::AudioStreamInfo(16, this->output_channels_(), rate));
  this->speaker_->start();
#endif
}

void SipClient::stop_speaker_() {
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) this->speaker_->stop();
#endif
}

void SipClient::start_mic_() {
#ifdef USE_MICROPHONE
  if (this->mic_source_ == nullptr) return;
  auto info = this->mic_source_->get_audio_stream_info();
  this->mic_rate_ = info.get_sample_rate();
  this->mic_channels_ = info.get_channels();
  this->mic_bits_ = info.get_bits_per_sample();
  this->mic_source_->start();
#endif
}

void SipClient::stop_mic_() {
#ifdef USE_MICROPHONE
  if (this->mic_source_ != nullptr) this->mic_source_->stop();
#endif
}

void SipClient::start_talking() {
#if defined(USE_MICROPHONE) && defined(USE_SPEAKER)
  if (!this->half_duplex_ || !this->media_active_ || this->talking_) return;
  // Hand the shared bus from speaker to mic.
  this->stop_speaker_();
  delay(150);
  this->start_mic_();
  this->talking_ = true;
  ESP_LOGD(TAG, "PTT: talking");
#endif
}

void SipClient::stop_talking() {
#if defined(USE_MICROPHONE) && defined(USE_SPEAKER)
  if (!this->half_duplex_ || !this->media_active_ || !this->talking_) return;
  // Hand the shared bus back from mic to speaker.
  this->stop_mic_();
  delay(150);
  this->start_speaker_();
  this->talking_ = false;
  ESP_LOGD(TAG, "PTT: listening");
#endif
}

#ifdef USE_MICROPHONE
void SipClient::on_mic_data_(const std::vector<uint8_t> &data) {
  if (!this->media_active_ || this->state_ != SIP_IN_CALL) return;
  if (!this->tx_allowed_) return;  // the peer holds us: nothing is sent
  if (this->half_duplex_ && !this->talking_) return;  // only transmit while PTT is held

  std::vector<int16_t> pcm16;
  if (this->mic_bits_ == 32) {
    const int32_t *samples32 = reinterpret_cast<const int32_t *>(data.data());
    size_t n32 = data.size() / sizeof(int32_t);
    pcm16.reserve(n32);
    for (size_t i = 0; i < n32; i++) {
      pcm16.push_back((int16_t)(samples32[i] >> 16));
    }
  } else {
    const int16_t *samples16 = reinterpret_cast<const int16_t *>(data.data());
    size_t n16 = data.size() / sizeof(int16_t);
    pcm16.assign(samples16, samples16 + n16);
  }

  const int16_t *samples = pcm16.data();
  size_t n = pcm16.size();

  // Reduce to mono if needed.
  std::vector<int16_t> mono;
  if (this->mic_channels_ == 2) {
    mono.reserve(n / 2);
    for (size_t i = 0; i + 1 < n; i += 2) mono.push_back(samples[i]);
    samples = mono.data();
    n = mono.size();
  }

  const uint32_t codec_rate = this->active_codec_->desc().pcm_rate;
  std::vector<int16_t> resampled;

  if (this->mic_rate_ == codec_rate) {
    this->rtp_.push_tx_audio(samples, n);
    return;
  }
  if (this->mic_rate_ == codec_rate * 2) {
    resampler::downsample_2to1(samples, n, resampled);
    this->rtp_.push_tx_audio(resampled.data(), resampled.size());
    return;
  }
  if (this->mic_rate_ * 2 == codec_rate) {
    resampler::upsample_1to2(samples, n, resampled);
    this->rtp_.push_tx_audio(resampled.data(), resampled.size());
    return;
  }
  static uint32_t last_mic_rate_warn_ms = 0;
  uint32_t now = millis();
  if (now - last_mic_rate_warn_ms > 2000) {
    ESP_LOGW(TAG, "Mic rate %u Hz incompatible with codec %u Hz; dropping frame",
             static_cast<unsigned>(this->mic_rate_), static_cast<unsigned>(codec_rate));
    last_mic_rate_warn_ms = now;
  }
}
#endif

// ---------------- main loop ----------------

void SipClient::handle_packet_(const std::string &raw) {
  ESP_LOGV(TAG, "RX <<<\n%s", raw.c_str());
  SipMessage m = parse_sip_message(raw);
  if (m.is_request) {
    this->handle_request_(m, raw);
    return;
  }
  std::string cseq = m.header("CSeq");
  size_t sp = cseq.find(' ');
  std::string method = (sp != std::string::npos) ? cseq.substr(sp + 1) : "";
  // trim
  while (!method.empty() && (method.back() == ' ' || method.back() == '\r')) method.pop_back();

  if (method == "REGISTER") {
    this->handle_register_response_(m);
  } else if (method == "INVITE") {
    this->handle_invite_response_(m, raw);
  }
  // responses to BYE/CANCEL/ACK: nothing to do.
}

void SipClient::handle_link_down_() {
  this->transport_up_ = false;
  this->registered_ = false;
  bool was_in_call = false;
  switch (this->state_) {
    case SIP_INVITING:
    case SIP_RINGING_OUT:
    case SIP_INCOMING:
    case SIP_ANSWERING:
    case SIP_IN_CALL:
      // No BYE/CANCEL can reach the peer; just tear our side down.
      ESP_LOGW(TAG, "Signaling link lost during a call, ending it");
      was_in_call = true;
      this->stop_media_();
      this->reset_call_session_();
      this->d_call_id_.clear();
      this->d_local_.clear();
      this->d_remote_.clear();
      this->d_remote_target_.clear();
      this->dialog_routes_.clear();
      break;
    default:
      break;
  }
  this->set_state_(SIP_IDLE);
  if (was_in_call) this->ended_cb_.call();
  // Re-register after a short pause, not at the (stale) refresh time.
  this->next_register_ms_ = millis() + 1000;
}

void SipClient::loop() {
  if (!network::is_connected()) {
    this->transport_->network_down();
    return;
  }
  this->transport_->loop();

  if (!this->transport_->connected()) {
    if (this->transport_up_) {
      ESP_LOGW(TAG, "SIP transport disconnected");
      this->handle_link_down_();
    }
    if (!this->transport_->is_open()) {
      if (millis() < this->next_register_ms_) return;
      if (!this->transport_->open(this->server_, this->server_port_)) {
        this->next_register_ms_ = millis() + 5000;
        return;
      }
    }
    // An asynchronous transport may still be connecting; try again next loop.
    if (!this->transport_->connected()) return;
  } else if (this->transport_up_ && this->transport_->generation() != this->transport_generation_) {
    // Dropped and reconnected between two loops: the old registration is gone.
    ESP_LOGW(TAG, "SIP transport reconnected");
    this->handle_link_down_();
  }
  if (!this->transport_up_) {
    if (millis() < this->next_register_ms_) return;
    this->transport_up_ = true;
    this->transport_generation_ = this->transport_->generation();
    this->do_register_();
  }

  // receive signaling
  std::string raw;
  for (int guard = 0; guard < 8; guard++) {
    if (!this->transport_->poll(raw)) break;
    this->handle_packet_(raw);
  }

  // registration refresh / retry
  if (this->state_ == SIP_REGISTERED && millis() >= this->next_register_ms_) {
    this->do_register_();  // periodic refresh
  } else if (this->state_ == SIP_REGISTERING && millis() >= this->next_register_ms_) {
    ESP_LOGW(TAG, "REGISTER timed out, retrying");
    this->do_register_();  // no response, retry
  }

  // RFC 4028 §10: the peer is the session refresher; if no refresh arrives in
  // time the dialog is dead on its side, so end the call like hangup() does.
  if (this->session_armed_ && this->state_ == SIP_IN_CALL &&
      static_cast<int32_t>(millis() - this->session_deadline_ms_) >= 0) {
    ESP_LOGW(TAG, "Session timer expired without a refresh from the peer, ending the call");
    this->hangup();
  }

  // media pump
  if (this->media_active_) this->rtp_.loop();
}

}  // namespace sip_client
}  // namespace esphome
