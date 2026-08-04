#pragma once
#include "codec.h"
#include "g711.h"

namespace esphome {
namespace sip_client {

// Stateless G.711 µ-law / A-law codec. `pt` is the negotiated wire PT (static
// 0/8 or a dynamic PT); encoding is selected by AudioCodecId, not by PT.
class G711Codec : public Codec {
 public:
  G711Codec(uint8_t pt, AudioCodecId id) : id_(id) {
    desc_.pt = pt;
    desc_.rtpmap = (id == AudioCodecId::PCMA) ? "PCMA/8000" : "PCMU/8000";
    desc_.pcm_rate = 8000;
    desc_.pcm_samples_per_frame = 160;
    desc_.payload_bytes = 160;
    desc_.ts_per_frame = 160;
  }

  const CodecDesc &desc() const override { return desc_; }
  AudioCodecId id() const override { return id_; }

  size_t encode(const int16_t *pcm, size_t samples, uint8_t *out) override {
    const bool alaw = this->id_ == AudioCodecId::PCMA;
    for (size_t i = 0; i < samples; i++) {
      out[i] = alaw ? g711::linear_to_alaw(pcm[i]) : g711::linear_to_ulaw(pcm[i]);
    }
    return samples;
  }

  size_t decode(const uint8_t *in, size_t bytes, int16_t *pcm) override {
    const bool alaw = this->id_ == AudioCodecId::PCMA;
    for (size_t i = 0; i < bytes; i++) {
      pcm[i] = alaw ? g711::alaw_to_linear(in[i]) : g711::ulaw_to_linear(in[i]);
    }
    return bytes;
  }

 private:
  AudioCodecId id_;
  CodecDesc desc_{};
};

inline std::unique_ptr<Codec> make_g711_codec(uint8_t pt, AudioCodecId id) {
  return std::unique_ptr<Codec>(new G711Codec(pt, id));
}

}  // namespace sip_client
}  // namespace esphome
