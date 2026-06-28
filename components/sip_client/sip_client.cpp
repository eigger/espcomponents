#include "sip_client.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/util.h"
#include "esphome/components/audio/audio.h"
#include "sip_auth.h"
#include "audio_resampler.h"

namespace esphome {
namespace sip_client {

static const char *const TAG = "sip_client";
static const char *const USER_AGENT = "ESPHome-sip_client";

static std::string trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

static std::string extract_angle_uri(const std::string &value) {
  size_t lt = value.find('<');
  size_t gt = value.find('>');
  if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
    return value.substr(lt + 1, gt - lt - 1);
  }
  std::string stripped = trim(value);
  if (stripped.rfind("sip", 0) == 0) {
    return stripped;
  }
  return "";
}

// Render an IPv4 sockaddr to dotted-quad without depending on inet_ntop.
static std::string sockaddr_ip(const struct sockaddr_storage &ss, uint16_t *port) {
  if (ss.ss_family != AF_INET) return "";
  const auto *s = reinterpret_cast<const struct sockaddr_in *>(&ss);
  uint32_t a = ntohl(s->sin_addr.s_addr);
  if (port != nullptr) *port = ntohs(s->sin_port);
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (a >> 24) & 0xFF, (a >> 16) & 0xFF, (a >> 8) & 0xFF,
           a & 0xFF);
  return std::string(buf);
}

void SipClient::setup() {
  std::srand(micros());
  if (this->domain_.empty()) this->domain_ = this->server_;
  this->recv_buf_.resize(2048);

  if (this->mic_ != nullptr) {
    this->mic_->add_data_callback(
        [this](const std::vector<uint8_t> &data) { this->on_mic_data_(data); });
  }
  ESP_LOGCONFIG(TAG, "sip_client initialized (server=%s:%u user=%s)", this->server_.c_str(),
                this->server_port_, this->username_.c_str());
}

void SipClient::dump_config() {
  ESP_LOGCONFIG(TAG, "SIP Client:");
  ESP_LOGCONFIG(TAG, "  Server: %s:%u", this->server_.c_str(), this->server_port_);
  ESP_LOGCONFIG(TAG, "  Username: %s", this->username_.c_str());
  ESP_LOGCONFIG(TAG, "  Domain: %s", this->domain_.c_str());
  ESP_LOGCONFIG(TAG, "  Local RTP port: %u", this->local_rtp_port_);
  ESP_LOGCONFIG(TAG, "  Channel: %s", this->channel_ == SIP_CH_MONO ? "mono" : "stereo");
  ESP_LOGCONFIG(TAG, "  Half-duplex (PTT): %s", this->half_duplex_ ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Microphone: %s", this->mic_ ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Speaker: %s", this->speaker_ ? "yes" : "no");
}

bool SipClient::open_socket_() {
  this->socket_ = socket::socket_ip(SOCK_DGRAM, IPPROTO_UDP);
  if (!this->socket_) {
    ESP_LOGW(TAG, "Could not create SIP socket");
    return false;
  }
  struct sockaddr_storage server_addr;
  socklen_t sl = socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&server_addr),
                                      sizeof(server_addr), this->server_.c_str(),
                                      this->server_port_);
  if (sl == 0) {
    ESP_LOGW(TAG, "Invalid server address '%s' (use an IP)", this->server_.c_str());
    this->socket_.reset();
    return false;
  }
  if (this->socket_->connect(reinterpret_cast<struct sockaddr *>(&server_addr), sl) != 0) {
    ESP_LOGW(TAG, "SIP connect failed");
    this->socket_.reset();
    return false;
  }
  this->socket_->setblocking(false);

  // Learn our source IP/port (for Via/Contact/SDP) from the connected socket.
  struct sockaddr_storage local;
  socklen_t ll = sizeof(local);
  this->socket_->getsockname(reinterpret_cast<struct sockaddr *>(&local), &ll);
  this->local_ip_ = sockaddr_ip(local, &this->local_port_);
  if (this->local_ip_.empty() || this->local_ip_ == "0.0.0.0") {
    this->local_ip_ = network::get_use_address();
  }
  ESP_LOGI(TAG, "SIP socket bound, local %s:%u", this->local_ip_.c_str(), this->local_port_);
  return true;
}

void SipClient::send_raw_(const std::string &msg) {
  if (!this->socket_) return;
  this->socket_->write(msg.data(), msg.size());
  ESP_LOGV(TAG, "TX >>>\n%s", msg.c_str());
}

// Prefer PCMU, then PCMA; default to PCMU.
static uint8_t choose_payload_(const SdpInfo &sdp) {
  if (sdp.pcmu_pt >= 0) return (uint8_t) sdp.pcmu_pt;
  if (sdp.pcma_pt >= 0) return (uint8_t) sdp.pcma_pt;
  return 0;
}

void SipClient::set_state_(SipState s) {
  if (this->state_ != s) {
    ESP_LOGD(TAG, "state %d -> %d", this->state_, s);
    this->state_ = s;
  }
}

// ---------------- registration ----------------

void SipClient::do_register_() {
  this->reg_call_id_ = gen_call_id(this->local_ip_);
  this->reg_tag_ = gen_tag();
  this->reg_branch_ = gen_branch();
  this->reg_cseq_++;
  this->register_auth_tried_ = false;
  this->send_raw_(this->build_register_());
  this->set_state_(SIP_REGISTERING);
  this->next_register_ms_ = millis() + 5000;  // retry window if no response
}

std::string SipClient::contact_uri_() {
  char buf[128];
  snprintf(buf, sizeof(buf), "<sip:%s@%s:%u>", this->username_.c_str(), this->local_ip_.c_str(),
           this->local_port_);
  return buf;
}

std::string SipClient::build_register_() {
  std::string aor = "sip:" + this->username_ + "@" + this->domain_;
  std::string reg_uri = "sip:" + this->domain_;
  std::string disp = this->caller_id_.empty() ? this->username_ : this->caller_id_;
  std::string msg;
  msg += "REGISTER " + reg_uri + " SIP/2.0\r\n";
  msg += "Via: SIP/2.0/UDP " + this->local_ip_ + ":" + std::to_string(this->local_port_) +
         ";branch=" + this->reg_branch_ + ";rport\r\n";
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
    ESP_LOGD(TAG, "Ignoring REGISTER response for old CSeq %u", cseq_num);
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
    std::string resp = digest_response(this->username_, this->password_, realm, "REGISTER", uri,
                                       nonce, qop.empty() ? "" : "auth", nc, cnonce);

    this->reg_cseq_++;
    this->reg_branch_ = gen_branch();
    std::string msg = this->build_register_();
    // insert Authorization before Content-Length
    std::string auth = std::string(proxy ? "Proxy-Authorization: " : "Authorization: ") +
                       "Digest username=\"" + this->username_ + "\", realm=\"" + realm +
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
  this->d_call_id_ = gen_call_id(this->local_ip_);
  this->d_local_tag_ = gen_tag();
  this->d_branch_ = gen_branch();
  this->d_cseq_ = 1;
  std::string disp = this->caller_id_.empty() ? this->username_ : this->caller_id_;
  this->d_local_ = "\"" + disp + "\" <sip:" + this->username_ + "@" + this->domain_ +
                   ">;tag=" + this->d_local_tag_;
  this->d_remote_ = "<sip:" + number + "@" + this->domain_ + ">";
  this->d_remote_target_ = "sip:" + number + "@" + this->domain_;
  this->send_raw_(this->build_invite_());
  this->set_state_(SIP_INVITING);
  ESP_LOGI(TAG, "Calling %s", number.c_str());
}

std::string SipClient::local_sdp_() {
  std::string id = std::to_string(millis());
  std::string sdp;
  sdp += "v=0\r\n";
  sdp += "o=- " + id + " " + id + " IN IP4 " + this->local_ip_ + "\r\n";
  sdp += "s=esphome\r\n";
  sdp += "c=IN IP4 " + this->local_ip_ + "\r\n";
  sdp += "t=0 0\r\n";
  sdp += "m=audio " + std::to_string(this->local_rtp_port_) + " RTP/AVP 0 8 101\r\n";
  sdp += "a=rtpmap:0 PCMU/8000\r\n";
  sdp += "a=rtpmap:8 PCMA/8000\r\n";
  sdp += "a=rtpmap:101 telephone-event/8000\r\n";
  sdp += "a=fmtp:101 0-15\r\n";
  sdp += "a=ptime:20\r\n";
  sdp += "a=sendrecv\r\n";
  return sdp;
}

std::string SipClient::build_invite_() {
  std::string sdp = this->local_sdp_();
  std::string msg;
  msg += "INVITE " + this->d_remote_target_ + " SIP/2.0\r\n";
  msg += "Via: SIP/2.0/UDP " + this->local_ip_ + ":" + std::to_string(this->local_port_) +
         ";branch=" + this->d_branch_ + ";rport\r\n";
  msg += "Max-Forwards: 70\r\n";
  msg += "From: " + this->d_local_ + "\r\n";
  msg += "To: " + this->d_remote_ + "\r\n";
  msg += "Call-ID: " + this->d_call_id_ + "\r\n";
  msg += "CSeq: " + std::to_string(this->d_cseq_) + " INVITE\r\n";
  msg += "Contact: " + this->contact_uri_() + "\r\n";
  msg += "User-Agent: " + std::string(USER_AGENT) + "\r\n";
  msg += "Content-Type: application/sdp\r\n";
  msg += "Content-Length: " + std::to_string(sdp.size()) + "\r\n\r\n";
  msg += sdp;
  return msg;
}

std::string SipClient::build_ack_(const SipMessage &resp) {
  std::string to = resp.header("To");
  std::string contact = resp.header("Contact");
  std::string target = extract_angle_uri(contact);
  if (target.empty()) {
    target = this->d_remote_target_;
  }

  uint32_t cseq = (uint32_t) std::atoi(resp.header("CSeq").c_str());
  if (cseq == 0) {
    cseq = this->d_cseq_;
  }

  std::string msg;
  msg += "ACK " + target + " SIP/2.0\r\n";
  msg += "Via: SIP/2.0/UDP " + this->local_ip_ + ":" + std::to_string(this->local_port_) +
         ";branch=" + gen_branch() + ";rport\r\n";
  msg += "Max-Forwards: 70\r\n";
  msg += "From: " + this->d_local_ + "\r\n";
  msg += "To: " + (to.empty() ? this->d_remote_ : to) + "\r\n";
  msg += "Call-ID: " + this->d_call_id_ + "\r\n";
  msg += "CSeq: " + std::to_string(cseq) + " ACK\r\n";
  msg += "Content-Length: 0\r\n\r\n";
  return msg;
}

void SipClient::handle_invite_response_(const SipMessage &m, const std::string &raw) {
  if (!this->outbound_) return;

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
    std::string resp = digest_response(this->username_, this->password_, realm, "INVITE", uri,
                                       nonce, qop.empty() ? "" : "auth", nc, cnonce);
    this->d_cseq_++;
    this->d_branch_ = gen_branch();
    std::string msg = this->build_invite_();
    std::string auth = std::string(proxy ? "Proxy-Authorization: " : "Authorization: ") +
                       "Digest username=\"" + this->username_ + "\", realm=\"" + realm +
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
    // Capture remote tag and target, parse SDP, ACK, start media.
    std::string to = m.header("To");
    if (!to.empty()) this->d_remote_ = to;
    std::string contact = m.header("Contact");
    std::string target = extract_angle_uri(contact);
    if (!target.empty())
      this->d_remote_target_ = target;

    SdpInfo sdp = parse_sdp(m.body);
    this->remote_rtp_ip_ = sdp.connection_ip.empty() ? this->remote_rtp_ip_ : sdp.connection_ip;
    this->remote_rtp_port_ = sdp.audio_port;
    this->chosen_pt_ = choose_payload_(sdp);
    this->remote_dtmf_pt_ = sdp.telephone_event_pt;

    this->send_raw_(this->build_ack_(m));
    this->start_media_();
    this->set_state_(SIP_IN_CALL);
    ESP_LOGI(TAG, "Call connected");
    this->connected_cb_.call();
    return;
  }

  // >= 300 final failure: ACK and end.
  this->send_raw_(this->build_ack_(m));
  ESP_LOGW(TAG, "Call failed: %d %s", m.status_code, m.reason.c_str());
  this->stop_media_();
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
                                       bool with_sdp) {
  std::string to = req.header("To");
  if (to.find("tag=") == std::string::npos) to += ";tag=" + this->d_local_tag_;
  std::string sdp = with_sdp ? this->local_sdp_() : "";
  std::string msg;
  msg += "SIP/2.0 " + std::to_string(code) + " " + reason + "\r\n";
  msg += "Via: " + req.header("Via") + "\r\n";
  msg += "From: " + req.header("From") + "\r\n";
  msg += "To: " + to + "\r\n";
  msg += "Call-ID: " + req.header("Call-ID") + "\r\n";
  msg += "CSeq: " + req.header("CSeq") + "\r\n";
  if (code >= 200 && code < 300 && req.method == "INVITE")
    msg += "Contact: " + this->contact_uri_() + "\r\n";
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

  if (method == "INVITE") {
    if (this->state_ != SIP_REGISTERED) {
      this->send_raw_(this->build_response_(m, 486, "Busy Here", false));
      return;
    }
    // New inbound dialog.
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
    this->d_cseq_ = std::atoi(m.header("CSeq").c_str());

    SdpInfo sdp = parse_sdp(m.body);
    this->remote_rtp_ip_ = sdp.connection_ip;
    this->remote_rtp_port_ = sdp.audio_port;
    this->chosen_pt_ = choose_payload_(sdp);
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
    if (this->state_ == SIP_ANSWERING) {
      this->set_state_(SIP_IN_CALL);
      ESP_LOGI(TAG, "Call connected (inbound)");
      this->connected_cb_.call();
    }
    return;
  }

  if (method == "BYE") {
    this->send_raw_(this->build_response_(m, 200, "OK", false));
    ESP_LOGI(TAG, "Remote hung up");
    this->stop_media_();
    this->set_state_(SIP_REGISTERED);
    this->ended_cb_.call();
    return;
  }

  if (method == "CANCEL") {
    this->send_raw_(this->build_response_(m, 200, "OK", false));
    if (this->state_ == SIP_INCOMING) {
      this->send_raw_(this->build_response_(this->incoming_invite_, 487, "Request Terminated", false));
      this->set_state_(SIP_REGISTERED);
      this->ended_cb_.call();
    }
    return;
  }

  if (method == "OPTIONS") {
    this->send_raw_(this->build_response_(m, 200, "OK", false));
    return;
  }

  // Unknown in-dialog request: acknowledge.
  this->send_raw_(this->build_response_(m, 200, "OK", false));
}

void SipClient::answer() {
  if (this->state_ != SIP_INCOMING) {
    ESP_LOGW(TAG, "answer() ignored in state %d", this->state_);
    return;
  }
  this->start_media_();
  this->send_raw_(this->build_response_(this->incoming_invite_, 200, "OK", true));
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
      this->set_state_(SIP_REGISTERED);
      this->ended_cb_.call();
      break;
    }
    case SIP_INVITING:
    case SIP_RINGING_OUT: {
      // CANCEL the pending INVITE (same branch/cseq).
      std::string msg;
      msg += "CANCEL " + this->d_remote_target_ + " SIP/2.0\r\n";
      msg += "Via: SIP/2.0/UDP " + this->local_ip_ + ":" + std::to_string(this->local_port_) +
             ";branch=" + this->d_branch_ + ";rport\r\n";
      msg += "Max-Forwards: 70\r\n";
      msg += "From: " + this->d_local_ + "\r\n";
      msg += "To: " + this->d_remote_ + "\r\n";
      msg += "Call-ID: " + this->d_call_id_ + "\r\n";
      msg += "CSeq: " + std::to_string(this->d_cseq_) + " CANCEL\r\n";
      msg += "Content-Length: 0\r\n\r\n";
      this->send_raw_(msg);
      this->stop_media_();
      this->set_state_(SIP_REGISTERED);
      this->ended_cb_.call();
      break;
    }
    case SIP_INCOMING: {
      this->send_raw_(this->build_response_(this->incoming_invite_, 603, "Decline", false));
      this->set_state_(SIP_REGISTERED);
      this->ended_cb_.call();
      break;
    }
    default:
      break;
  }
}

std::string SipClient::build_request_in_dialog_(const std::string &method) {
  std::string msg;
  msg += method + " " + this->d_remote_target_ + " SIP/2.0\r\n";
  msg += "Via: SIP/2.0/UDP " + this->local_ip_ + ":" + std::to_string(this->local_port_) +
         ";branch=" + gen_branch() + ";rport\r\n";
  msg += "Max-Forwards: 70\r\n";
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
  this->rtp_.set_payload_type(this->chosen_pt_);
  this->rtp_.set_dtmf_payload_type(this->remote_dtmf_pt_);
  this->rtp_.set_remote(this->remote_rtp_ip_, this->remote_rtp_port_);
  this->rtp_.set_on_audio([this](const int16_t *pcm, size_t n) {
    if (this->speaker_ == nullptr) return;
    if (this->half_duplex_ && this->talking_) return;  // speaker is off while transmitting

    const int16_t *play_pcm = pcm;
    size_t play_n = n;
    std::vector<int16_t> upsampled;

    if (this->speaker_rate_ >= 16000) {
      resampler::upsample_1to2(pcm, n, upsampled);
      play_pcm = upsampled.data();
      play_n = upsampled.size();
    }

    if (this->channel_ == SIP_CH_STEREO) {
      // Duplicate mono samples to stereo (L/R) for stereo mixers/speakers (e.g. Voice PE).
      std::vector<int16_t> stereo(play_n * 2);
      for (size_t i = 0; i < play_n; i++) {
        stereo[i * 2] = play_pcm[i];
        stereo[i * 2 + 1] = play_pcm[i];
      }
      this->speaker_->play(reinterpret_cast<const uint8_t *>(stereo.data()), stereo.size() * sizeof(int16_t));
    } else {
      // Mono output (e.g. es8311): push samples as-is.
      this->speaker_->play(reinterpret_cast<const uint8_t *>(play_pcm), play_n * sizeof(int16_t));
    }
  });
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
  ESP_LOGI(TAG, "Media started: remote %s:%u pt=%u dtmf_pt=%d (mic %u Hz/%uch/%ubits)%s",
           this->remote_rtp_ip_.c_str(), this->remote_rtp_port_, this->chosen_pt_,
           this->remote_dtmf_pt_, this->mic_rate_, this->mic_channels_, this->mic_bits_,
           this->half_duplex_ ? " [half-duplex: listening]" : "");
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
  if (this->speaker_ == nullptr) return;
  this->speaker_rate_ = 8000;
  this->speaker_->set_audio_stream_info(audio::AudioStreamInfo(16, this->output_channels_(), 8000));
  this->speaker_->start();
}

void SipClient::stop_speaker_() {
  if (this->speaker_ != nullptr) this->speaker_->stop();
}

void SipClient::start_mic_() {
  if (this->mic_ == nullptr) return;
  auto info = this->mic_->get_audio_stream_info();
  this->mic_rate_ = info.get_sample_rate();
  this->mic_channels_ = info.get_channels();
  this->mic_bits_ = info.get_bits_per_sample();
  this->mic_->start();
}

void SipClient::stop_mic_() {
  if (this->mic_ != nullptr) this->mic_->stop();
}

void SipClient::start_talking() {
  if (!this->half_duplex_ || !this->media_active_ || this->talking_) return;
  // Hand the shared bus from speaker to mic.
  this->stop_speaker_();
  delay(150);
  this->start_mic_();
  this->talking_ = true;
  ESP_LOGD(TAG, "PTT: talking");
}

void SipClient::stop_talking() {
  if (!this->half_duplex_ || !this->media_active_ || !this->talking_) return;
  // Hand the shared bus back from mic to speaker.
  this->stop_mic_();
  delay(150);
  this->start_speaker_();
  this->talking_ = false;
  ESP_LOGD(TAG, "PTT: listening");
}

void SipClient::on_mic_data_(const std::vector<uint8_t> &data) {
  if (!this->media_active_ || this->state_ != SIP_IN_CALL) return;
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

  if (this->mic_rate_ >= 16000) {
    std::vector<int16_t> down;
    resampler::downsample_2to1(samples, n, down);
    this->rtp_.push_tx_audio(down.data(), down.size());
  } else {
    this->rtp_.push_tx_audio(samples, n);
  }
}

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

void SipClient::loop() {
  if (!network::is_connected()) return;

  if (!this->socket_) {
    if (millis() < this->next_register_ms_) return;
    if (!this->open_socket_()) {
      this->next_register_ms_ = millis() + 5000;
      return;
    }
    this->do_register_();
  }

  // receive signaling
  for (int guard = 0; guard < 8; guard++) {
    ssize_t len = this->socket_->read(this->recv_buf_.data(), this->recv_buf_.size() - 1);
    if (len <= 0) break;
    this->recv_buf_[len] = 0;
    std::string raw(reinterpret_cast<char *>(this->recv_buf_.data()), len);
    this->handle_packet_(raw);
  }

  // registration refresh / retry
  if (this->state_ == SIP_REGISTERED && millis() >= this->next_register_ms_) {
    this->do_register_();  // periodic refresh
  } else if (this->state_ == SIP_REGISTERING && millis() >= this->next_register_ms_) {
    ESP_LOGW(TAG, "REGISTER timed out, retrying");
    this->do_register_();  // no response, retry
  }

  // media pump
  if (this->media_active_) this->rtp_.loop();
}

}  // namespace sip_client
}  // namespace esphome
