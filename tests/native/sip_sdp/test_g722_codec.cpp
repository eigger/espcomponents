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
  // Several frames: encode+decode QMF has ~22 samples of group delay. An
  // unaligned 20 ms 1 kHz tone scores ~-5 dB even when the codec is correct.
  const size_t n = 320 * 4;
  std::vector<int16_t> pcm(n);
  for (size_t i = 0; i < n; i++) {
    pcm[i] = (int16_t) (8000.0 * std::sin(2.0 * 3.141592653589793 * 1000.0 * i / 16000.0));
  }
  std::vector<uint8_t> encoded(n / 2);
  std::vector<int16_t> decoded(n);
  require_eq((int) codec.encode(pcm.data(), n, encoded.data()), (int) (n / 2), "g722 encode size");
  require_eq((int) codec.decode(encoded.data(), n / 2, decoded.data()), (int) n, "g722 decode size");

  constexpr size_t k_skip = 160;  // ignore QMF startup transient
  constexpr size_t k_max_delay = 40;
  double best_snr = -1e9;
  for (size_t delay = 0; delay <= k_max_delay; delay++) {
    double err = 0, signal = 0;
    for (size_t i = k_skip; i + delay < n; i++) {
      double ref = (double) pcm[i];
      double e = ref - (double) decoded[i + delay];
      err += e * e;
      signal += ref * ref;
    }
    double snr = 10.0 * std::log10(signal / (err + 1e-12));
    if (snr > best_snr) best_snr = snr;
  }
  if (best_snr < 20.0) {
    std::cerr << "FAIL: G722 SNR too low: " << best_snr << " dB\n";
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
