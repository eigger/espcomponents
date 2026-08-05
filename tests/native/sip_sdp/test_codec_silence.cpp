// Verifies the silence frame that RtpSession sends to keep the RTP stream
// running when there is nothing to transmit (no microphone configured, or
// half-duplex while listening). RtpSession builds it by pushing a frame of
// zeroed PCM through the negotiated codec, so this checks that doing so
// produces a well-formed, actually-silent payload for every codec.

#include "g711_codec.h"
#include "g722_codec.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using esphome::sip_client::AudioCodecId;
using esphome::sip_client::Codec;
using esphome::sip_client::make_g711_codec;
using esphome::sip_client::make_g722_codec;

namespace {

int g_failures = 0;

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    g_failures++;
  }
}

void require_eq(long actual, long expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got " << actual << ", expected " << expected << ")\n";
    g_failures++;
  }
}

// Mirrors RtpSession::send_silence_packet_(): one frame of zeroed PCM encoded
// through the codec.
std::vector<uint8_t> encode_silence_frame(Codec &codec) {
  const auto &desc = codec.desc();
  std::vector<int16_t> silence_pcm(desc.pcm_samples_per_frame, 0);
  std::vector<uint8_t> out(desc.payload_bytes, 0);
  size_t written = codec.encode(silence_pcm.data(), silence_pcm.size(), out.data());
  out.resize(written);
  return out;
}

// A silence frame must decode back to (near) zero PCM. G.722 is ADPCM, so its
// decoder converges rather than returning exact zeros; allow a small band.
void check_decodes_to_silence(Codec &codec, const std::vector<uint8_t> &payload, int tolerance,
                              const char *label) {
  const auto &desc = codec.desc();
  std::vector<int16_t> pcm(desc.max_pcm_samples_for_payload(payload.size()) + 16, 0);
  size_t samples = codec.decode(payload.data(), payload.size(), pcm.data());
  require(samples > 0, "silence payload decodes to some PCM");

  int peak = 0;
  for (size_t i = 0; i < samples; i++) {
    int magnitude = std::abs(static_cast<int>(pcm[i]));
    if (magnitude > peak) peak = magnitude;
  }
  if (peak > tolerance) {
    std::cerr << "FAIL: " << label << " decoded peak " << peak << " exceeds tolerance " << tolerance
              << '\n';
    g_failures++;
  }
}

void test_g711_silence(AudioCodecId id, uint8_t expected_byte, const char *label) {
  auto codec = make_g711_codec(/*pt=*/id == AudioCodecId::PCMA ? 8 : 0, id);
  auto payload = encode_silence_frame(*codec);

  require_eq(static_cast<long>(payload.size()), codec->desc().payload_bytes,
             "G.711 silence frame is a full payload");

  bool all_expected = !payload.empty();
  for (uint8_t b : payload) {
    if (b != expected_byte) all_expected = false;
  }
  require(all_expected, label);

  check_decodes_to_silence(*codec, payload, /*tolerance=*/16, label);
}

void test_g722_silence() {
  auto codec = make_g722_codec(/*pt=*/9);
  codec->reset();

  // Feed several consecutive silence frames: the encoder is stateful, so a
  // steady silent input must stay silent rather than drifting.
  for (int frame = 0; frame < 10; frame++) {
    auto payload = encode_silence_frame(*codec);
    require_eq(static_cast<long>(payload.size()), codec->desc().payload_bytes,
               "G.722 silence frame is a full payload");
  }

  // Decode a fresh silence frame with a fresh decoder and confirm it is quiet.
  auto encoder = make_g722_codec(9);
  auto decoder = make_g722_codec(9);
  encoder->reset();
  decoder->reset();
  int peak = 0;
  for (int frame = 0; frame < 10; frame++) {
    auto payload = encode_silence_frame(*encoder);
    std::vector<int16_t> pcm(encoder->desc().pcm_samples_per_frame + 16, 0);
    size_t samples = decoder->decode(payload.data(), payload.size(), pcm.data());
    for (size_t i = 0; i < samples; i++) {
      int magnitude = std::abs(static_cast<int>(pcm[i]));
      if (magnitude > peak) peak = magnitude;
    }
  }
  // Well under any audible level; a real signal would be orders of magnitude larger.
  if (peak > 64) {
    std::cerr << "FAIL: G.722 silence decoded peak " << peak << " exceeds tolerance 64\n";
    g_failures++;
  }
}

// The silence frame must never overflow the RTP stack buffer RtpSession sizes
// to MAX_AUDIO_PAYLOAD_BYTES.
void test_payload_fits_rtp_buffer() {
  auto pcmu = make_g711_codec(0, AudioCodecId::PCMU);
  auto pcma = make_g711_codec(8, AudioCodecId::PCMA);
  auto g722 = make_g722_codec(9);
  for (Codec *codec : {static_cast<Codec *>(pcmu.get()), static_cast<Codec *>(pcma.get()),
                       static_cast<Codec *>(g722.get())}) {
    require(codec->desc().payload_bytes <= esphome::sip_client::MAX_AUDIO_PAYLOAD_BYTES,
            "codec payload fits MAX_AUDIO_PAYLOAD_BYTES");
  }
}

}  // namespace

int main() {
  test_g711_silence(AudioCodecId::PCMU, 0xFF, "PCMU silence is 0xFF");
  test_g711_silence(AudioCodecId::PCMA, 0xD5, "PCMA silence is 0xD5");
  test_g722_silence();
  test_payload_fits_rtp_buffer();

  if (g_failures != 0) {
    std::cerr << g_failures << " codec_silence test(s) failed\n";
    return 1;
  }
  std::cout << "All codec_silence tests passed\n";
  return 0;
}
