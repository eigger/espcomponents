#include "rtp_session.h"
#include <cstdlib>
#include <cstring>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "g711.h"

namespace esphome {
namespace sip_client {

static const char *const TAG = "sip_client.rtp";

static const size_t SAMPLES_PER_FRAME = 160;   // 20 ms @ 8 kHz
static const uint32_t FRAME_MS = 20;
static const uint32_t DTMF_TONE_SAMPLES = 8 * SAMPLES_PER_FRAME;  // ~160 ms tone
static const int DTMF_END_PACKETS = 3;
static const size_t TX_BUFFER_MAX = 8000;  // 1 s of audio, drop excess

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
  this->socket_ = socket::socket_ip(SOCK_DGRAM, IPPROTO_UDP);
  if (!this->socket_) {
    ESP_LOGW(TAG, "Could not create RTP socket");
    return false;
  }
  struct sockaddr_storage local_addr;
  socklen_t sl = socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&local_addr),
                                          sizeof(local_addr), local_port);
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
  this->last_tx_ms_ = millis();
  this->recv_buf_.resize(1500);
  ESP_LOGI(TAG, "RTP started on port %u (pt=%u, dtmf_pt=%d)", local_port, this->payload_type_,
           this->dtmf_pt_);
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
}

void RtpSession::push_tx_audio(const int16_t *pcm, size_t samples) {
  if (!this->socket_) return;
  std::lock_guard<std::mutex> lock(this->tx_mutex_);
  if (this->tx_buffer_.size() + samples > TX_BUFFER_MAX) {
    // Drop oldest to bound latency.
    size_t overflow = this->tx_buffer_.size() + samples - TX_BUFFER_MAX;
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
  uint8_t packet[12 + SAMPLES_PER_FRAME];
  {
    std::lock_guard<std::mutex> lock(this->tx_mutex_);
    if (this->tx_buffer_.size() < SAMPLES_PER_FRAME) return;
    this->build_rtp_header_(packet, this->first_packet_, this->payload_type_, this->timestamp_);
    for (size_t i = 0; i < SAMPLES_PER_FRAME; i++) {
      int16_t s = this->tx_buffer_[i];
      packet[12 + i] = (this->payload_type_ == 8) ? g711::linear_to_alaw(s) : g711::linear_to_ulaw(s);
    }
    this->tx_buffer_.erase(this->tx_buffer_.begin(), this->tx_buffer_.begin() + SAMPLES_PER_FRAME);
  }
  this->socket_->sendto(packet, sizeof(packet), 0,
                        reinterpret_cast<struct sockaddr *>(&this->remote_addr_),
                        this->remote_addr_len_);
  this->seq_++;
  this->timestamp_ += SAMPLES_PER_FRAME;
  this->first_packet_ = false;
}

void RtpSession::send_dtmf_packet_() {
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

  bool end = this->dtmf_duration_ >= DTMF_TONE_SAMPLES;
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
      this->timestamp_ = this->dtmf_timestamp_ + this->dtmf_duration_ + SAMPLES_PER_FRAME;
      this->first_packet_ = true;  // re-mark audio after DTMF
    }
  } else {
    this->dtmf_duration_ += SAMPLES_PER_FRAME;
  }
}

void RtpSession::receive_() {
  if (!this->socket_) return;
  for (int guard = 0; guard < 8; guard++) {
    ssize_t len = this->socket_->read(this->recv_buf_.data(), this->recv_buf_.size());
    if (len < 12) return;  // EAGAIN or runt packet
    uint8_t pt = this->recv_buf_[1] & 0x7F;
    bool marker = (this->recv_buf_[1] & 0x80) != 0;
    size_t header_len = 12 + 4 * (this->recv_buf_[0] & 0x0F);  // CSRC count
    if ((size_t) len <= header_len) continue;

    if (this->dtmf_pt_ >= 0 && pt == (uint8_t) this->dtmf_pt_) {
      if (marker && this->on_dtmf_) {
        int event = this->recv_buf_[header_len];
        char c = '?';
        if (event <= 9) c = '0' + event;
        else if (event == 10) c = '*';
        else if (event == 11) c = '#';
        else if (event <= 15) c = 'A' + (event - 12);
        this->on_dtmf_(c);
      }
      continue;
    }
    if (pt != 0 && pt != 8) continue;  // unknown codec

    size_t n = len - header_len;
    this->decode_buf_.resize(n);
    for (size_t i = 0; i < n; i++) {
      uint8_t b = this->recv_buf_[header_len + i];
      this->decode_buf_[i] = (pt == 8) ? g711::alaw_to_linear(b) : g711::ulaw_to_linear(b);
    }
    if (this->on_audio_) this->on_audio_(this->decode_buf_.data(), n);
  }
}

void RtpSession::loop() {
  if (!this->socket_ || !this->remote_set_) return;
  this->receive_();

  uint32_t now = millis();
  if (now - this->last_tx_ms_ > 500) {
    ESP_LOGD(TAG, "RTP sender lagging behind by %u ms; resetting pacing", now - this->last_tx_ms_);
    this->last_tx_ms_ = now;
    std::lock_guard<std::mutex> lock(this->tx_mutex_);
    this->tx_buffer_.clear();
  }

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
        if (this->tx_buffer_.size() >= SAMPLES_PER_FRAME) {
          has_enough_samples = true;
        }
      }
      if (has_enough_samples) {
        this->send_audio_packet_();
        this->last_tx_ms_ += FRAME_MS;
        packets_sent++;
      } else {
        break;
      }
    }
  }
}

}  // namespace sip_client
}  // namespace esphome
