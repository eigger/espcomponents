#pragma once
#include <cstdint>
#include <vector>

namespace esphome {
namespace sip_client {

// Minimal sample-rate conversion between 8 kHz (the G.711 / RTP rate) and the
// rate of the ESPHome microphone/speaker (commonly 16 kHz). Only integer 1:1
// and 2:1 ratios are handled, which covers the realistic 8 k / 16 k cases.

namespace resampler {

// 16 kHz -> 8 kHz: average pairs of samples (cheap anti-alias).
static inline void downsample_2to1(const int16_t *in, size_t in_samples, std::vector<int16_t> &out) {
  out.clear();
  out.reserve(in_samples / 2);
  for (size_t i = 0; i + 1 < in_samples; i += 2) {
    out.push_back((int16_t) (((int32_t) in[i] + (int32_t) in[i + 1]) / 2));
  }
}

// 8 kHz -> 16 kHz: linear interpolation between samples.
static inline void upsample_1to2(const int16_t *in, size_t in_samples, std::vector<int16_t> &out) {
  out.clear();
  out.reserve(in_samples * 2);
  for (size_t i = 0; i < in_samples; i++) {
    int16_t cur = in[i];
    int16_t next = (i + 1 < in_samples) ? in[i + 1] : cur;
    out.push_back(cur);
    out.push_back((int16_t) (((int32_t) cur + (int32_t) next) / 2));
  }
}

}  // namespace resampler

}  // namespace sip_client
}  // namespace esphome
