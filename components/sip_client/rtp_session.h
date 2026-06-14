#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace sip_client {

// RTP audio session for a single G.711 call. Owns one UDP socket, paces packet
// transmission at 20 ms (160 samples @ 8 kHz), decodes received audio, and can
// emit RFC 2833 telephone-event (DTMF) packets. All PCM exchanged with the
// caller is signed 16-bit linear at 8 kHz, mono.
class RtpSession {
 public:
  // payload_type: 0 = PCMU (mu-law), 8 = PCMA (A-law).
  void set_payload_type(uint8_t pt) { this->payload_type_ = pt; }
  void set_dtmf_payload_type(int pt) { this->dtmf_pt_ = pt; }
  void set_remote(const std::string &ip, uint16_t port);

  void set_on_audio(std::function<void(const int16_t *, size_t)> &&cb) { this->on_audio_ = std::move(cb); }
  void set_on_dtmf(std::function<void(char)> &&cb) { this->on_dtmf_ = std::move(cb); }

  bool start(uint16_t local_port);
  void stop();
  bool is_running() const { return this->socket_ != nullptr; }

  // Append captured PCM (8 kHz) for transmission.
  void push_tx_audio(const int16_t *pcm, size_t samples);
  // Queue DTMF digits ('0'-'9','*','#','A'-'D') for RFC 2833 transmission.
  void queue_dtmf(const std::string &digits);

  void loop();

 protected:
  void send_audio_packet_();
  void send_dtmf_packet_();
  void receive_();
  void build_rtp_header_(uint8_t *buf, bool marker, uint8_t pt, uint32_t timestamp);

  std::unique_ptr<socket::Socket> socket_{nullptr};
  struct sockaddr_storage remote_addr_;
  socklen_t remote_addr_len_{0};
  bool remote_set_{false};

  uint8_t payload_type_{0};
  int dtmf_pt_{101};

  uint16_t seq_{0};
  uint32_t timestamp_{0};
  uint32_t ssrc_{0};
  bool first_packet_{true};

  std::vector<int16_t> tx_buffer_;
  std::mutex tx_mutex_;
  uint32_t last_tx_ms_{0};

  // DTMF state
  std::string dtmf_queue_;
  bool dtmf_active_{false};
  int dtmf_event_{-1};
  uint16_t dtmf_duration_{0};
  uint32_t dtmf_timestamp_{0};
  int dtmf_end_packets_{0};

  std::function<void(const int16_t *, size_t)> on_audio_{};
  std::function<void(char)> on_dtmf_{};

  std::vector<uint8_t> recv_buf_;
  std::vector<int16_t> decode_buf_;
};

}  // namespace sip_client
}  // namespace esphome
