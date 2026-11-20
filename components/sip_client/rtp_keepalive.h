#pragma once
#include <cstdint>

namespace esphome {
namespace sip_client {

// Decides when RtpSession sends a silence frame while it has no audio to send
// (receive-only device, half-duplex while listening, start of a call).
//
// period_ms == 0 keeps the classic behaviour: a silence frame every 20 ms, a
// continuous stream. A non-zero period thins the idle stream to one frame per
// period (after an initial burst) so a remote device does not spend ~80 kbit/s
// of upstream on silence. The packets still refresh the NAT mapping of the RTP
// port and let a PBX with symmetric RTP (Asterisk rtp_symmetric) latch onto the
// device's public address. Pure logic, no I/O, wrap-safe for millis().
class RtpKeepalive {
 public:
  // Continuous silence (every 20 ms slot) after reset() and after real audio,
  // before the thinned period applies. Asterisk's strict RTP needs `probation`
  // (default 4) consecutive packets from the new public source before it
  // latches, so this is deliberately generous: it survives packet loss and an
  // inbound call whose first packets arrive before the PBX processed the SDP.
  static constexpr uint32_t START_BURST_MS = 2000;
  static constexpr uint32_t AUDIO_BURST_MS = 750;

  void configure(uint32_t period_ms) { this->period_ms_ = period_ms; }
  uint32_t period_ms() const { return this->period_ms_; }
  bool thinned() const { return this->period_ms_ != 0; }

  // Call when the session starts.
  void reset(uint32_t now) {
    this->last_sent_ms_ = now;
    this->burst_start_ms_ = now;
    this->burst_len_ms_ = START_BURST_MS;
  }

  // Any RTP packet left the socket (audio, silence, DTMF).
  void on_sent(uint32_t now) { this->last_sent_ms_ = now; }

  // Real audio was sent; the next idle phase starts with a short burst.
  void on_audio(uint32_t now) {
    this->last_sent_ms_ = now;
    this->burst_start_ms_ = now;
    this->burst_len_ms_ = AUDIO_BURST_MS;
  }

  // True when an idle silence frame should be sent for this 20 ms slot.
  bool idle_frame_due(uint32_t now) const {
    if (this->period_ms_ == 0) return true;
    if (static_cast<uint32_t>(now - this->burst_start_ms_) < this->burst_len_ms_) return true;
    return static_cast<uint32_t>(now - this->last_sent_ms_) >= this->period_ms_;
  }

 private:
  uint32_t period_ms_{0};
  uint32_t last_sent_ms_{0};
  uint32_t burst_start_ms_{0};
  uint32_t burst_len_ms_{0};
};

}  // namespace sip_client
}  // namespace esphome
