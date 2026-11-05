#include "tone_meter.h"

#include <cmath>

namespace lcr {

void ToneMeter::begin(float freqHz, float sampleRateHz) {
  coeff_ = 2.0f * std::cos(2.0f * 3.14159265358979f * freqHz / sampleRateHz);
  maxAmp_ = 0.0f;
  blocks_ = 0;
}

void ToneMeter::addBlock(const float* x, size_t n) {
  if (n == 0) return;
  float s1 = 0.0f, s2 = 0.0f;
  for (size_t i = 0; i < n; i++) {
    const float s = x[i] + coeff_ * s1 - s2;
    s2 = s1;
    s1 = s;
  }
  const float power = s1 * s1 + s2 * s2 - coeff_ * s1 * s2;
  const float amp = 2.0f * std::sqrt(power > 0.0f ? power : 0.0f) / static_cast<float>(n);
  if (amp > maxAmp_) maxAmp_ = amp;
  blocks_++;
}

void ToneMeter::addBlock(const int32_t* samples, size_t n) {
  if (n == 0) return;
  float s1 = 0.0f, s2 = 0.0f;
  for (size_t i = 0; i < n; i++) {
    const float x = static_cast<float>(samples[i]) / 2147483648.0f;
    const float s = x + coeff_ * s1 - s2;
    s2 = s1;
    s1 = s;
  }
  const float power = s1 * s1 + s2 * s2 - coeff_ * s1 * s2;
  const float amp = 2.0f * std::sqrt(power > 0.0f ? power : 0.0f) / static_cast<float>(n);
  if (amp > maxAmp_) maxAmp_ = amp;
  blocks_++;
}

float ToneMeter::maxDb() const {
  if (blocks_ == 0) return -120.0f;
  const float db = 20.0f * std::log10(maxAmp_ + 1e-6f);
  return db < -120.0f ? -120.0f : db;
}

}  // namespace lcr
