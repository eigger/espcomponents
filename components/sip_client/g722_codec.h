#pragma once
#include "codec.h"
#include "g722_encoder.h"
#include "g722_decoder.h"

namespace esphome {
namespace sip_client {

// ITU G.722 wideband codec (64 kb/s). RTP clock is 8 kHz per RFC 3551; PCM I/O
// is 16 kHz mono. `pt` is the negotiated wire PT (static 9 or dynamic).
class G722Codec : public Codec {
 public:
  explicit G722Codec(uint8_t pt) : id_(AudioCodecId::G722) {
    desc_.pt = pt;
    desc_.rtpmap = "G722/8000";
    desc_.pcm_rate = 16000;
    desc_.pcm_samples_per_frame = 320;
    desc_.payload_bytes = 160;
    desc_.ts_per_frame = 160;
    this->enc_ = g722_encoder_new(64000, G722_DEFAULT);
    this->dec_ = g722_decoder_new(64000, G722_DEFAULT);
  }

  ~G722Codec() override { this->destroy_state_(); }

  const CodecDesc &desc() const override { return desc_; }
  AudioCodecId id() const override { return id_; }

  size_t encode(const int16_t *pcm, size_t samples, uint8_t *out) override {
    if (this->enc_ == nullptr) return 0;
    int n = g722_encode(this->enc_, pcm, (int) samples, out);
    return n > 0 ? (size_t) n : 0;
  }

  size_t decode(const uint8_t *in, size_t bytes, int16_t *pcm) override {
    if (this->dec_ == nullptr) return 0;
    int n = g722_decode(this->dec_, in, (int) bytes, pcm);
    return n > 0 ? (size_t) n : 0;
  }

  void reset() override {
    this->destroy_state_();
    this->enc_ = g722_encoder_new(64000, G722_DEFAULT);
    this->dec_ = g722_decoder_new(64000, G722_DEFAULT);
  }

 private:
  void destroy_state_() {
    if (this->enc_ != nullptr) {
      g722_encoder_destroy(this->enc_);
      this->enc_ = nullptr;
    }
    if (this->dec_ != nullptr) {
      g722_decoder_destroy(this->dec_);
      this->dec_ = nullptr;
    }
  }

  AudioCodecId id_;
  CodecDesc desc_{};
  G722_ENC_CTX *enc_{nullptr};
  G722_DEC_CTX *dec_{nullptr};
};

inline std::unique_ptr<Codec> make_g722_codec(uint8_t pt) {
  return std::unique_ptr<Codec>(new G722Codec(pt));
}

}  // namespace sip_client
}  // namespace esphome
