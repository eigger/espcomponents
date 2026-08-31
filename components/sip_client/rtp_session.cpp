#include "rtp_session.h"
#include <cstdlib>
#include <cstring>
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sip_client {

static const char *const TAG = "sip_client.rtp";

static const uint32_t FRAME_MS = 20;
static const int DTMF_END_PACKETS = 3;
// How long the sender waits for real audio before it starts filling the stream
// with silence. Long enough to ride out a microphone buffer boundary (so live
// speech is never chopped up), short enough that a session with nothing to send
// starts transmitting almost immediately.
static const uint32_t SILENCE_GRACE_MS = 60;

// Bind-any sockaddr for the given family. Unlike socket::set_sockaddr_any(),
// this doesn't depend on ESPHome's global network::enable_ipv6 setting — the
// caller picks the family to match the (already-known) remote peer address.
static socklen_t set_sockaddr_any_family(struct sockaddr *addr, socklen_t addrlen, sa_family_t family,
                                         uint16_t port) {
#if USE_NETWORK_IPV6
  if (family == AF_INET6) {
    if (addrlen < sizeof(struct sockaddr_in6)) return 0;
    auto *a6 = reinterpret_cast<struct sockaddr_in6 *>(addr);
    memset(a6, 0, sizeof(struct sockaddr_in6));
    a6->sin6_family = AF_INET6;
    a6->sin6_port = htons(port);
    return sizeof(struct sockaddr_in6);
  }
#else
  (void) family;
#endif
  if (addrlen < sizeof(struct sockaddr_in)) return 0;
  auto *a4 = reinterpret_cast<struct sockaddr_in *>(addr);
  memset(a4, 0, sizeof(struct sockaddr_in));
  a4->sin_family = AF_INET;
  a4->sin_port = htons(port);
  return sizeof(struct sockaddr_in);
}

static int dtmf_char_to_event(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c == '*') return 10;
  if (c == '#') return 11;
  if (c >= 'A' && c <= 'D') return 12 + (c - 'A');
  if (c >= 'a' && c <= 'd') return 12 + (c - 'a');
  return -1;
}

void RtpSession::set_remote(const std::string &ip, uint16_t port) {
  this->remote_addr_len_ =
      socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&this->remote_addr_),
                           sizeof(this->remote_addr_), ip.c_str(), port);
  this->remote_set_ = this->remote_addr_len_ > 0;
}

bool RtpSession::start(uint16_t local_port) {
  this->stop();
  if (this->codec_ == nullptr) {
    ESP_LOGW(TAG, "RTP start failed: no codec");
    return false;
  }
  this->codec_->reset();
  // Match the remote peer's address family (set via set_remote() before this
  // call) instead of socket::socket_ip(), which is fixed to AF_INET6 whenever
  // ESPHome's global network::enable_ipv6 is on — see open_socket_() in
  // sip_client.cpp for the same fix and why it's needed.
  sa_family_t family = this->remote_set_ ? this->remote_addr_.ss_family : AF_INET;
  this->socket_ = socket::socket(family, SOCK_DGRAM, IPPROTO_UDP);
  if (!this->socket_) {
    ESP_LOGW(TAG, "Could not create RTP socket");
    return false;
  }
  struct sockaddr_storage local_addr;
  socklen_t sl = set_sockaddr_any_family(reinterpret_cast<struct sockaddr *>(&local_addr),
                                        sizeof(local_addr), family, local_port);
  if (this->socket_->bind(reinterpret_cast<struct sockaddr *>(&local_addr), sl) != 0) {
    ESP_LOGW(TAG, "RTP bind failed on port %u", local_port);
    this->socket_.reset();
    return false;
  }
  this->socket_->setblocking(false);

  this->seq_ = (uint16_t) (std::rand() & 0xFFFF);
  this->timestamp_ = (uint32_t) std::rand();
  this->ssrc_ = (uint32_t) std::rand();
  this->first_packet_ = true;
  {
    std::lock_guard<std::mutex> lock(this->tx_mutex_);
    this->tx_buffer_.clear();
  }
  this->dtmf_queue_.clear();
  this->dtmf_active_ = false;
  this->dtmf_rx_.reset();
  this->last_tx_ms_ = millis();
  // Treat the session start as "just sent audio" so the grace period applies
  // from here; a session with no microphone starts emitting silence right after.
  this->last_audio_tx_ms_ = this->last_tx_ms_;
  this->silence_pcm_.assign(this->pcm_samples_per_frame_(), 0);
  this->recv_buf_.resize(1500);
  ESP_LOGI(TAG, "RTP started on port %u (pt=%u %s, dtmf_pt=%d)", local_port,
           this->codec_->desc().pt, this->codec_->desc().rtpmap, this->dtmf_pt_);
  return true;
}

void RtpSession::stop() {
  if (this->socket_) {
    this->socket_->close();
    this->socket_.reset();
  }
  {
    std::lock_guard<std::mutex> lock(this->tx_mutex_);
    this->tx_buffer_.clear();
  }
  this->dtmf_queue_.clear();
  this->dtmf_active_ = false;
  this->dtmf_rx_.reset();
}

void RtpSession::push_tx_audio(const int16_t *pcm, size_t samples) {
  if (!this->socket_) return;
  // ~1 s of PCM at the codec sample rate (8000 for G.711).
  const size_t tx_max =
      this->codec_ != nullptr ? static_cast<size_t>(this->codec_->desc().pcm_rate) : 8000;
  std::lock_guard<std::mutex> lock(this->tx_mutex_);
  if (this->tx_buffer_.size() + samples > tx_max) {
    // Drop oldest to bound latency.
    size_t overflow = this->tx_buffer_.size() + samples - tx_max;
    if (overflow >= this->tx_buffer_.size())
      this->tx_buffer_.clear();
    else
      this->tx_buffer_.erase(this->tx_buffer_.begin(), this->tx_buffer_.begin() + overflow);
  }
  this->tx_buffer_.insert(this->tx_buffer_.end(), pcm, pcm + samples);
}

void RtpSession::queue_dtmf(const std::string &digits) {
  if (this->dtmf_pt_ < 0) {
    ESP_LOGW(TAG, "Remote did not offer telephone-event; DTMF dropped");
    return;
  }
  this->dtmf_queue_ += digits;
}

void RtpSession::build_rtp_header_(uint8_t *buf, bool marker, uint8_t pt, uint32_t timestamp) {
  buf[0] = 0x80;  // V=2, P=0, X=0, CC=0
  buf[1] = (marker ? 0x80 : 0x00) | (pt & 0x7F);
  buf[2] = (this->seq_ >> 8) & 0xFF;
  buf[3] = this->seq_ & 0xFF;
  buf[4] = (timestamp >> 24) & 0xFF;
  buf[5] = (timestamp >> 16) & 0xFF;
  buf[6] = (timestamp >> 8) & 0xFF;
  buf[7] = timestamp & 0xFF;
  buf[8] = (this->ssrc_ >> 24) & 0xFF;
  buf[9] = (this->ssrc_ >> 16) & 0xFF;
  buf[10] = (this->ssrc_ >> 8) & 0xFF;
  buf[11] = this->ssrc_ & 0xFF;
}

void RtpSession::send_audio_packet_() {
  if (this->codec_ == nullptr) return;
  const uint16_t pcm_n = this->pcm_samples_per_frame_();
  const uint16_t pay_n = this->payload_bytes_();
  const uint16_t ts_step = this->ts_per_frame_();
  uint8_t packet[12 + MAX_AUDIO_PAYLOAD_BYTES];
  if (pay_n > MAX_AUDIO_PAYLOAD_BYTES) {
    ESP_LOGW(TAG, "codec payload %u exceeds MAX_AUDIO_PAYLOAD_BYTES (%u); drop", pay_n,
             static_cast<unsigned>(MAX_AUDIO_PAYLOAD_BYTES));
    return;
  }
  size_t written = 0;
  {
    std::lock_guard<std::mutex> lock(this->tx_mutex_);
    if (this->tx_buffer_.size() < pcm_n) return;
    this->build_rtp_header_(packet, this->first_packet_, this->codec_->desc().pt, this->timestamp_);
    written = this->codec_->encode(this->tx_buffer_.data(), pcm_n, packet + 12);
    this->tx_buffer_.erase(this->tx_buffer_.begin(), this->tx_buffer_.begin() + pcm_n);
  }
  if (written == 0) {
    static uint32_t last_encode_zero_ms = 0;
    uint32_t now = millis();
    if (now - last_encode_zero_ms > 2000) {
      ESP_LOGW(TAG, "encode produced 0 bytes; packet dropped");
      last_encode_zero_ms = now;
    }
    return;
  }
  if (written != pay_n) {
    static uint32_t last_encode_mismatch_ms = 0;
    uint32_t now = millis();
    if (now - last_encode_mismatch_ms > 2000) {
      ESP_LOGW(TAG, "encode size mismatch: %u vs expected %u; sending written size",
               static_cast<unsigned>(written), pay_n);
      last_encode_mismatch_ms = now;
    }
  }
  if (written > MAX_AUDIO_PAYLOAD_BYTES) written = MAX_AUDIO_PAYLOAD_BYTES;
  this->socket_->sendto(packet, 12 + written, 0,
                        reinterpret_cast<struct sockaddr *>(&this->remote_addr_),
                        this->remote_addr_len_);
  this->seq_++;
  this->timestamp_ += ts_step;
  this->first_packet_ = false;
  this->last_audio_tx_ms_ = millis();
}

// Sends one frame of digital silence, keeping the RTP stream running when there
// is nothing to transmit. The zero PCM is pushed through the codec rather than
// writing a constant payload byte: G.722 is stateful ADPCM, so its encoder has
// to see the samples to stay in step with the far end's decoder (for G.711 this
// simply yields the usual 0xFF / 0xD5 silence).
void RtpSession::send_silence_packet_() {
  if (this->codec_ == nullptr) return;
  const uint16_t pcm_n = this->pcm_samples_per_frame_();
  const uint16_t pay_n = this->payload_bytes_();
  const uint16_t ts_step = this->ts_per_frame_();
  if (pay_n > MAX_AUDIO_PAYLOAD_BYTES || this->silence_pcm_.size() < pcm_n) return;

  uint8_t packet[12 + MAX_AUDIO_PAYLOAD_BYTES];
  this->build_rtp_header_(packet, this->first_packet_, this->codec_->desc().pt, this->timestamp_);
  size_t written = this->codec_->encode(this->silence_pcm_.data(), pcm_n, packet + 12);
  if (written == 0) return;
  if (written > MAX_AUDIO_PAYLOAD_BYTES) written = MAX_AUDIO_PAYLOAD_BYTES;
  this->socket_->sendto(packet, 12 + written, 0,
                        reinterpret_cast<struct sockaddr *>(&this->remote_addr_),
                        this->remote_addr_len_);
  this->seq_++;
  this->timestamp_ += ts_step;
  this->first_packet_ = false;
}

void RtpSession::send_dtmf_packet_() {
  const uint16_t ts_step = this->ts_per_frame_();
  const uint32_t dtmf_tone_samples = 8 * ts_step;  // ~160 ms

  if (!this->dtmf_active_) {
    if (this->dtmf_queue_.empty()) return;
    int event = dtmf_char_to_event(this->dtmf_queue_.front());
    this->dtmf_queue_.erase(this->dtmf_queue_.begin());
    if (event < 0) return;
    this->dtmf_active_ = true;
    this->dtmf_event_ = event;
    this->dtmf_duration_ = 0;
    this->dtmf_end_packets_ = 0;
    this->dtmf_timestamp_ = this->timestamp_;
  }

  bool end = this->dtmf_duration_ >= dtmf_tone_samples;
  uint8_t packet[16];
  this->build_rtp_header_(packet, this->dtmf_duration_ == 0, (uint8_t) this->dtmf_pt_,
                          this->dtmf_timestamp_);
  packet[12] = (uint8_t) this->dtmf_event_;
  packet[13] = (end ? 0x80 : 0x00) | 0x0A;  // E bit + volume 10
  packet[14] = (this->dtmf_duration_ >> 8) & 0xFF;
  packet[15] = this->dtmf_duration_ & 0xFF;
  this->socket_->sendto(packet, sizeof(packet), 0,
                        reinterpret_cast<struct sockaddr *>(&this->remote_addr_),
                        this->remote_addr_len_);
  this->seq_++;

  if (end) {
    this->dtmf_end_packets_++;
    if (this->dtmf_end_packets_ >= DTMF_END_PACKETS) {
      this->dtmf_active_ = false;
      this->timestamp_ = this->dtmf_timestamp_ + this->dtmf_duration_ + ts_step;
      this->first_packet_ = true;  // re-mark audio after DTMF
    }
  } else {
    this->dtmf_duration_ += ts_step;
  }
}

void RtpSession::receive_() {
  if (!this->socket_ || this->codec_ == nullptr) return;
  const uint8_t expect_pt = this->codec_->desc().pt;
  for (int guard = 0; guard < 8; guard++) {
    ssize_t len = this->socket_->read(this->recv_buf_.data(), this->recv_buf_.size());
    if (len < 12) return;  // EAGAIN or runt packet
    uint8_t pt = this->recv_buf_[1] & 0x7F;
    size_t header_len = 12 + 4 * (this->recv_buf_[0] & 0x0F);  // CSRC count
    if ((size_t) len <= header_len) continue;

    if (this->dtmf_pt_ >= 0 && pt == (uint8_t) this->dtmf_pt_) {
      // RFC 4733 payload: event, E|R|volume, duration (16 bit).
      if ((size_t) len >= header_len + 4 && this->on_dtmf_) {
        uint32_t event_ts = ((uint32_t) this->recv_buf_[4] << 24) |
                            ((uint32_t) this->recv_buf_[5] << 16) |
                            ((uint32_t) this->recv_buf_[6] << 8) | this->recv_buf_[7];
        char c = this->dtmf_rx_.feed(this->recv_buf_[header_len], event_ts);
        if (c != 0) this->on_dtmf_(c);
      }
      continue;
    }
    if (pt != expect_pt) {
      // Throttle: unexpected PT (e.g. comfort noise 13) otherwise leaves silence
      // with no log trail.
      static uint32_t last_unexpected_ms = 0;
      uint32_t now = millis();
      if (now - last_unexpected_ms > 2000) {
        ESP_LOGW(TAG, "Ignoring RTP pt=%u (negotiated pt=%u)", pt, expect_pt);
        last_unexpected_ms = now;
      }
      continue;
    }
    if (!this->on_audio_) continue;  // send-only / no speaker: skip decode

    size_t n = len - header_len;
    const size_t pcm_cap = this->codec_->desc().max_pcm_samples_for_payload(n);
    this->decode_buf_.resize(pcm_cap);
    size_t samples = this->codec_->decode(this->recv_buf_.data() + header_len, n,
                                          this->decode_buf_.data());
    this->decode_buf_.resize(samples);
    this->on_audio_(this->decode_buf_.data(), samples);
  }
}

void RtpSession::loop() {
  if (!this->socket_ || !this->remote_set_) return;
  this->receive_();

  uint32_t now = millis();
  if (now - this->last_tx_ms_ > 500) {
    bool tx_empty = true;
    {
      std::lock_guard<std::mutex> lock(this->tx_mutex_);
      tx_empty = this->tx_buffer_.empty();
    }
    if (!tx_empty) {
      ESP_LOGD(TAG, "RTP sender lagging behind by %u ms; resetting pacing",
               static_cast<unsigned>(now - this->last_tx_ms_));
      std::lock_guard<std::mutex> lock(this->tx_mutex_);
      this->tx_buffer_.clear();
    }
    this->last_tx_ms_ = now;
  }

  const uint16_t pcm_n = this->pcm_samples_per_frame_();
  int packets_sent = 0;
  while (now - this->last_tx_ms_ >= FRAME_MS && packets_sent < 5) {
    if (this->dtmf_active_ || !this->dtmf_queue_.empty()) {
      this->send_dtmf_packet_();
      this->last_tx_ms_ += FRAME_MS;
      packets_sent++;
    } else {
      bool has_enough_samples = false;
      {
        std::lock_guard<std::mutex> lock(this->tx_mutex_);
        if (this->tx_buffer_.size() >= pcm_n) {
          has_enough_samples = true;
        }
      }
      if (has_enough_samples) {
        this->send_audio_packet_();
      } else if (now - this->last_audio_tx_ms_ < SILENCE_GRACE_MS) {
        // The buffer only just ran dry — most likely a microphone chunk
        // boundary mid-speech. Wait for the real samples rather than splicing
        // silence into live audio.
        break;
      } else {
        // Nothing to send for a while: no microphone configured, or half-duplex
        // while listening. Keep the RTP stream running anyway, which is what a
        // real SIP phone does and what opens the return path. The outbound
        // packets hold the NAT/firewall mapping for our RTP port open, and
        // PBXes that latch onto the source address (Asterisk's rtp_symmetric,
        // on by default for NAT'd endpoints) only learn where to send audio
        // once they have seen a packet from us. Without this the peer's audio
        // never arrives at all.
        this->send_silence_packet_();
      }
      this->last_tx_ms_ += FRAME_MS;
      packets_sent++;
    }
  }
}

}  // namespace sip_client
}  // namespace esphome
