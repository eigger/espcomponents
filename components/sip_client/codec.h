#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

namespace esphome {
namespace sip_client {

// Identifies the audio encoding independently of the negotiated RTP payload
// type (which may be a dynamic PT such as 97 for PCMA).
enum class AudioCodecId : uint8_t {
  PCMU = 0,
  PCMA = 1,
};

struct CodecDesc {
  uint8_t pt{0};                      // negotiated wire payload type
  const char *rtpmap{"PCMU/8000"};    // SDP a=rtpmap encoding name/rate
  uint32_t pcm_rate{8000};            // linear PCM sample rate for I/O
  uint16_t pcm_samples_per_frame{160};
  uint16_t payload_bytes{160};
  uint16_t ts_per_frame{160};         // RTP timestamp clock increment
};

// Pluggable audio codec used by RtpSession. PCM is always signed 16-bit mono
// at desc().pcm_rate.
class Codec {
 public:
  virtual ~Codec() = default;
  virtual const CodecDesc &desc() const = 0;
  // Encode `samples` PCM frames into `out` (capacity >= desc().payload_bytes).
  // Returns bytes written.
  virtual size_t encode(const int16_t *pcm, size_t samples, uint8_t *out) = 0;
  // Decode `bytes` of payload into `pcm` (capacity >= samples produced).
  // Returns number of PCM samples written.
  virtual size_t decode(const uint8_t *in, size_t bytes, int16_t *pcm) = 0;
  virtual void reset() {}
  virtual AudioCodecId id() const = 0;
};

}  // namespace sip_client
}  // namespace esphome
