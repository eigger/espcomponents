#include "g711_codec.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using esphome::sip_client::AudioCodecId;
using esphome::sip_client::G711Codec;
using esphome::sip_client::make_g711_codec;

namespace {

int g_failures = 0;

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    g_failures++;
  }
}

void require_eq(int actual, int expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got " << actual << ", expected " << expected << ")\n";
    g_failures++;
  }
}

void test_desc_framing_constants() {
  G711Codec ulaw(0, AudioCodecId::PCMU);
  require_eq(ulaw.desc().pcm_samples_per_frame, 160, "ulaw pcm samples");
  require_eq(ulaw.desc().payload_bytes, 160, "ulaw payload");
  require_eq(ulaw.desc().ts_per_frame, 160, "ulaw ts");
  require_eq((int) ulaw.desc().pcm_rate, 8000, "ulaw rate");

  // Dynamic PT must not change framing or encoding family.
  auto alaw = make_g711_codec(97, AudioCodecId::PCMA);
  require_eq(alaw->desc().pt, 97, "dynamic pt stored");
  require(std::string(alaw->desc().rtpmap) == "PCMA/8000", "dynamic pcma rtpmap");
  require_eq(alaw->desc().pcm_samples_per_frame, 160, "alaw pcm samples");
  require_eq(alaw->desc().payload_bytes, 160, "alaw payload");
  require_eq(alaw->desc().ts_per_frame, 160, "alaw ts");
}

void test_roundtrip_snr(AudioCodecId id, const char *label) {
  G711Codec codec(id == AudioCodecId::PCMA ? 8 : 0, id);
  const size_t n = 160;
  std::vector<int16_t> pcm(n);
  for (size_t i = 0; i < n; i++) {
    // ~1 kHz tone at moderate amplitude (well inside G.711 range).
    pcm[i] = (int16_t) (8000.0 * std::sin(2.0 * 3.141592653589793 * 1000.0 * i / 8000.0));
  }
  std::vector<uint8_t> encoded(n);
  std::vector<int16_t> decoded(n);
  require_eq((int) codec.encode(pcm.data(), n, encoded.data()), (int) n, "encode size");
  require_eq((int) codec.decode(encoded.data(), n, decoded.data()), (int) n, "decode size");

  double err = 0, signal = 0;
  for (size_t i = 0; i < n; i++) {
    double e = (double) pcm[i] - (double) decoded[i];
    err += e * e;
    signal += (double) pcm[i] * (double) pcm[i];
  }
  double snr = 10.0 * std::log10(signal / (err + 1e-12));
  // G.711 SNR for a mid-level tone should be comfortably above 20 dB.
  if (snr < 25.0) {
    std::cerr << "FAIL: " << label << " SNR too low: " << snr << " dB\n";
    g_failures++;
  }
}

void test_dynamic_pt_uses_alaw_not_ulaw() {
  // Encode silence-ish sample with both; A-law and µ-law differ for most values.
  int16_t sample = 1234;
  G711Codec ulaw(0, AudioCodecId::PCMU);
  G711Codec alaw_dyn(97, AudioCodecId::PCMA);
  uint8_t u = 0, a = 0;
  ulaw.encode(&sample, 1, &u);
  alaw_dyn.encode(&sample, 1, &a);
  require(u != a, "dynamic PCMA must not encode as µ-law");
}

}  // namespace

int main() {
  test_desc_framing_constants();
  test_roundtrip_snr(esphome::sip_client::AudioCodecId::PCMU, "PCMU");
  test_roundtrip_snr(esphome::sip_client::AudioCodecId::PCMA, "PCMA");
  test_dynamic_pt_uses_alaw_not_ulaw();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "All g711_codec tests passed\n";
  return 0;
}
