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

double best_aligned_snr(const std::vector<int16_t> &pcm, const std::vector<int16_t> &decoded,
                        size_t skip, size_t max_delay, size_t *out_delay) {
  double best_snr = -1e9;
  size_t best_delay = 0;
  const size_t n = pcm.size();
  for (size_t delay = 0; delay <= max_delay; delay++) {
    double err = 0, signal = 0;
    for (size_t i = skip; i + delay < n; i++) {
      double ref = (double) pcm[i];
      double e = ref - (double) decoded[i + delay];
      err += e * e;
      signal += ref * ref;
    }
    double snr = 10.0 * std::log10(signal / (err + 1e-12));
    if (snr > best_snr) {
      best_snr = snr;
      best_delay = delay;
    }
  }
  if (out_delay != nullptr) *out_delay = best_delay;
  return best_snr;
}

// Goertzel power at `freq_hz` over samples[start, start+n).
double goertzel_power(const int16_t *samples, size_t n, double freq_hz, double sample_rate) {
  const double omega = 2.0 * 3.141592653589793 * freq_hz / sample_rate;
  const double coeff = 2.0 * std::cos(omega);
  double s0 = 0, s1 = 0, s2 = 0;
  for (size_t i = 0; i < n; i++) {
    s0 = (double) samples[i] + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return s1 * s1 + s2 * s2 - coeff * s1 * s2;
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

  double snr = best_aligned_snr(pcm, decoded, /*skip=*/160, /*max_delay=*/40, nullptr);
  if (snr < 20.0) {
    std::cerr << "FAIL: G722 SNR too low: " << snr << " dB\n";
    g_failures++;
  }
}

// Mirror rtp_session: 20 ms frames (320 PCM -> 160 bytes) with persistent codec
// state, using a wideband mix so a narrowband passthrough cannot pass.
void test_streaming_wideband() {
  G722Codec codec(9);
  constexpr size_t k_frame_pcm = 320;
  constexpr size_t k_frame_bytes = 160;
  constexpr size_t k_frames = 25;  // 500 ms
  constexpr size_t n = k_frame_pcm * k_frames;
  constexpr double k_pi = 3.141592653589793;

  std::vector<int16_t> pcm(n);
  for (size_t i = 0; i < n; i++) {
    const double t = (double) i / 16000.0;
    // 400 Hz + 3.4 kHz (in-band) + 6 kHz (upper QMF band; gone if 8 kHz codec).
    double s = 4000.0 * std::sin(2.0 * k_pi * 400.0 * t) +
               4000.0 * std::sin(2.0 * k_pi * 3400.0 * t) +
               4000.0 * std::sin(2.0 * k_pi * 6000.0 * t);
    if (s > 32767.0) s = 32767.0;
    if (s < -32768.0) s = -32768.0;
    pcm[i] = (int16_t) s;
  }

  std::vector<uint8_t> encoded(n / 2);
  std::vector<int16_t> decoded(n);
  size_t enc_off = 0, dec_off = 0;
  for (size_t f = 0; f < k_frames; f++) {
    size_t got_enc =
        codec.encode(pcm.data() + f * k_frame_pcm, k_frame_pcm, encoded.data() + enc_off);
    require_eq((int) got_enc, (int) k_frame_bytes, "streaming encode frame size");
    size_t got_dec =
        codec.decode(encoded.data() + enc_off, k_frame_bytes, decoded.data() + dec_off);
    require_eq((int) got_dec, (int) k_frame_pcm, "streaming decode frame size");
    enc_off += k_frame_bytes;
    dec_off += k_frame_pcm;
  }
  require_eq((int) enc_off, (int) (n / 2), "streaming total encoded bytes");
  require_eq((int) dec_off, (int) n, "streaming total decoded samples");

  size_t delay = 0;
  double snr = best_aligned_snr(pcm, decoded, /*skip=*/320, /*max_delay=*/40, &delay);
  if (snr < 20.0) {
    std::cerr << "FAIL: streaming G722 SNR too low: " << snr << " dB\n";
    g_failures++;
  }
  // SpanDSP QMF encode+decode group delay is ~22 samples.
  require(delay >= 18 && delay <= 26, "streaming QMF group delay ~22");

  // High-band check: 6 kHz tone energy vs the three injected tones. Equal
  // amplitudes → ratio ≈ 1/3 when the upper QMF band survives; an 8 kHz codec
  // (or accidental G722_SAMPLE_RATE_8000) collapses this toward 0.
  constexpr size_t k_analyze = 2048;
  const size_t start = 320;
  require(start + delay + k_analyze <= n, "streaming analysis window fits");
  const int16_t *seg = decoded.data() + start + delay;
  const double p400 = goertzel_power(seg, k_analyze, 400.0, 16000.0);
  const double p3400 = goertzel_power(seg, k_analyze, 3400.0, 16000.0);
  const double p6000 = goertzel_power(seg, k_analyze, 6000.0, 16000.0);
  const double highband_ratio = p6000 / (p400 + p3400 + p6000 + 1e-12);
  if (highband_ratio < 0.15) {
    std::cerr << "FAIL: G722 high-band energy too low: ratio=" << highband_ratio
              << " (6 kHz component missing — not wideband)\n";
    g_failures++;
  }
}

}  // namespace

int main() {
  test_desc_framing_constants();
  test_roundtrip_snr();
  test_streaming_wideband();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "All g722_codec tests passed\n";
  return 0;
}
