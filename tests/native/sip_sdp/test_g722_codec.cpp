#include "g722_codec.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using esphome::sip_client::AudioCodecId;
using esphome::sip_client::G722Codec;
using esphome::sip_client::make_g722_codec;

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
  G722Codec codec(9);
  require_eq(codec.desc().pt, 9, "g722 pt");
  require(std::string(codec.desc().rtpmap) == "G722/8000", "g722 rtpmap");
  require_eq((int) codec.desc().pcm_rate, 16000, "g722 pcm rate");
  require_eq(codec.desc().pcm_samples_per_frame, 320, "g722 pcm samples");
  require_eq(codec.desc().payload_bytes, 160, "g722 payload");
  require_eq(codec.desc().ts_per_frame, 160, "g722 ts");
  require_eq((int) codec.desc().max_pcm_samples_for_payload(160), 320, "g722 decode cap 2:1");
  require_eq((int) codec.id(), (int) AudioCodecId::G722, "g722 id");

  auto dynamic = make_g722_codec(97);
  require_eq(dynamic->desc().pt, 97, "dynamic g722 pt stored");
}

void test_roundtrip_snr() {
  G722Codec codec(9);
  const size_t n = 320;
  std::vector<int16_t> pcm(n);
  for (size_t i = 0; i < n; i++) {
    // ~1 kHz tone at moderate amplitude.
    pcm[i] = (int16_t) (8000.0 * std::sin(2.0 * 3.141592653589793 * 1000.0 * i / 16000.0));
  }
  std::vector<uint8_t> encoded(160);
  std::vector<int16_t> decoded(n);
  require_eq((int) codec.encode(pcm.data(), n, encoded.data()), 160, "g722 encode size");
  require_eq((int) codec.decode(encoded.data(), 160, decoded.data()), (int) n, "g722 decode size");

  double err = 0, signal = 0;
  for (size_t i = 0; i < n; i++) {
    double e = (double) pcm[i] - (double) decoded[i];
    err += e * e;
    signal += (double) pcm[i] * (double) pcm[i];
  }
  double snr = 10.0 * std::log10(signal / (err + 1e-12));
  if (snr < 20.0) {
    std::cerr << "FAIL: G722 SNR too low: " << snr << " dB\n";
    g_failures++;
  }
}

}  // namespace

int main() {
  test_desc_framing_constants();
  test_roundtrip_snr();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "All g722_codec tests passed\n";
  return 0;
}
