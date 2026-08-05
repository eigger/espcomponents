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

// Upper bound on a single RTP audio payload (bytes). Stack buffers and
// sendto sizes are capped to this; codecs that need more must raise it.
static constexpr size_t MAX_AUDIO_PAYLOAD_BYTES = 320;

struct CodecDesc {
  uint8_t pt{0};                      // negotiated wire payload type
  const char *rtpmap{"PCMU/8000"};    // SDP a=rtpmap encoding name/rate
  uint32_t pcm_rate{8000};            // linear PCM sample rate for I/O
  uint16_t pcm_samples_per_frame{160};
  uint16_t payload_bytes{160};
  uint16_t ts_per_frame{160};         // RTP timestamp clock increment

  // Decode output capacity for a payload of `n` bytes (G.711 1:1, G.722 2:1).
  size_t max_pcm_samples_for_payload(size_t n) const {
    if (this->payload_bytes == 0) return n;
    return (n * this->pcm_samples_per_frame + this->payload_bytes - 1) / this->payload_bytes;
  }
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
