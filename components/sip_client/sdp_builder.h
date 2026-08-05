#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "codec.h"

namespace esphome {
namespace sip_client {

// Host-testable SDP body builder (no ESPHome runtime dependency).
struct SdpMediaParams {
  std::string session_id;
  std::string local_ip;
  uint16_t rtp_port{7078};
  const char *direction{"sendrecv"};  // sendrecv / recvonly / sendonly
  bool answer{false};
  // Answer only: selected audio PT + rtpmap label (e.g. "PCMA/8000").
  int answer_pt{0};
  const char *answer_rtpmap{"PCMU/8000"};
  // Offer only: codecs in preference order (default PCMU then PCMA when empty).
  std::vector<AudioCodecId> offer_codecs;
  // telephone-event PT. Offer always includes 101 when dtmf_pt < 0 and !answer.
  // Answer includes DTMF only when dtmf_pt >= 0.
  int dtmf_pt{-1};
};

std::string build_sdp_body(const SdpMediaParams &p);

}  // namespace sip_client
}  // namespace esphome
