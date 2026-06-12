#pragma once
#include <cstdint>

namespace esphome {
namespace sip_client {

// ITU-T G.711 mu-law / A-law companding.
// PCM is signed 16-bit linear, 8 kHz. Encoded sample is one byte per sample.

namespace g711 {

// ---- mu-law (PCMU, payload type 0) ----
static inline uint8_t linear_to_ulaw(int16_t pcm) {
  const uint16_t BIAS = 0x84;
  const int16_t CLIP = 32635;
  uint8_t sign = (pcm >> 8) & 0x80;
  if (sign != 0) pcm = -pcm;
  if (pcm > CLIP) pcm = CLIP;
  pcm = (int16_t) (pcm + BIAS);
  uint8_t exponent = 7;
  for (uint16_t mask = 0x4000; (pcm & mask) == 0 && exponent > 0; exponent--, mask >>= 1) {
  }
  uint8_t mantissa = (pcm >> (exponent + 3)) & 0x0F;
  uint8_t ulaw = ~(sign | (exponent << 4) | mantissa);
  return ulaw;
}

static inline int16_t ulaw_to_linear(uint8_t ulaw) {
  ulaw = ~ulaw;
  uint8_t sign = ulaw & 0x80;
  uint8_t exponent = (ulaw >> 4) & 0x07;
  uint8_t mantissa = ulaw & 0x0F;
  int16_t sample = ((mantissa << 3) + 0x84) << exponent;
  sample -= 0x84;
  return sign ? -sample : sample;
}

// ---- A-law (PCMA, payload type 8) ----
static inline uint8_t linear_to_alaw(int16_t pcm) {
  const int16_t CLIP = 32635;
  uint8_t sign = ((~pcm) >> 8) & 0x80;
  if (sign == 0) pcm = -pcm;
  if (pcm > CLIP) pcm = CLIP;
  uint8_t alaw;
  if (pcm >= 256) {
    uint8_t exponent = 7;
    for (uint16_t mask = 0x4000; (pcm & mask) == 0 && exponent > 0; exponent--, mask >>= 1) {
    }
    uint8_t mantissa = (pcm >> (exponent + 3)) & 0x0F;
    alaw = (exponent << 4) | mantissa;
  } else {
    alaw = pcm >> 4;
  }
  return (alaw ^ sign ^ 0x55);
}

static inline int16_t alaw_to_linear(uint8_t alaw) {
  alaw ^= 0x55;
  uint8_t sign = alaw & 0x80;
  uint8_t exponent = (alaw >> 4) & 0x07;
  uint8_t mantissa = alaw & 0x0F;
  int16_t sample = (mantissa << 4) + 8;
  if (exponent != 0) {
    sample += 0x100;
    sample <<= (exponent - 1);
  }
  return sign ? sample : -sample;
}

}  // namespace g711

}  // namespace sip_client
}  // namespace esphome
