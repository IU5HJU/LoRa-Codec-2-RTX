#include "audio_frontend.h"

#include <cmath>
#include <cstring>

namespace lcr {

namespace {
constexpr float kPi = 3.14159265358979f;
}

void Biquad::setHighpass(float fs, float fc) {
  const float q = 0.70710678f;  // Butterworth
  const float w0 = 2.0f * kPi * fc / fs;
  const float cw = std::cos(w0);
  const float alpha = std::sin(w0) / (2.0f * q);
  const float a0 = 1.0f + alpha;
  b0_ = ((1.0f + cw) / 2.0f) / a0;
  b1_ = (-(1.0f + cw)) / a0;
  b2_ = ((1.0f + cw) / 2.0f) / a0;
  a1_ = (-2.0f * cw) / a0;
  a2_ = (1.0f - alpha) / a0;
  reset();
}

float Biquad::process(float x) {
  // Forma diretta trasposta II.
  const float y = b0_ * x + z1_;
  z1_ = b1_ * x - a1_ * y + z2_;
  z2_ = b2_ * x - a2_ * y;
  return y;
}

void Biquad::reset() {
  z1_ = 0;
  z2_ = 0;
}

void designLowpass(float* taps, size_t n, float cutoffHz, float sampleRate) {
  const float wc = 2.0f * cutoffHz / sampleRate;  // taglio normalizzato
  const int m = static_cast<int>((n - 1) / 2);
  float sum = 0;
  for (size_t j = 0; j < n; j++) {
    const float k = static_cast<float>(static_cast<int>(j) - m);
    const float sinc = (k == 0) ? 1.0f : std::sin(kPi * wc * k) / (kPi * wc * k);
    const float hamming = 0.54f - 0.46f * std::cos(2.0f * kPi * static_cast<float>(j) / static_cast<float>(n - 1));
    taps[j] = wc * sinc * hamming;
    sum += taps[j];
  }
  for (size_t j = 0; j < n; j++) taps[j] /= sum;
}

Decimator2::Decimator2() {
  designLowpass(taps_, kTaps, 3600.0f, static_cast<float>(kMicRate));
  reset();
}

void Decimator2::reset() { memset(hist_, 0, sizeof(hist_)); }

void Decimator2::process(const float* in, size_t n, float* out) {
  if (n > kMicSamplesPerFrame) n = kMicSamplesPerFrame;
  n &= ~static_cast<size_t>(1);
  float buf[kTaps - 1 + kMicSamplesPerFrame];
  memcpy(buf, hist_, sizeof(hist_));
  memcpy(buf + (kTaps - 1), in, n * sizeof(float));
  for (size_t k = 0; k < n / 2; k++) {
    float acc = 0;
    const size_t base = 2 * k + (kTaps - 1);
    for (size_t j = 0; j < kTaps; j++) acc += taps_[j] * buf[base - j];
    out[k] = acc;
  }
  memcpy(hist_, buf + n, sizeof(hist_));
}

void Agc::process(float* x, size_t n) {
  if (n == 0) return;
  double sumSq = 0;
  for (size_t i = 0; i < n; i++) sumSq += static_cast<double>(x[i]) * x[i];
  const float rms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(n)));
  const float rmsDb = 20.0f * std::log10(rms + 1e-9f);
  inputDb_ = rmsDb;

  const float before = gainDb_;
  if (rmsDb > cfg_.gateDbfs) {
    float desired = cfg_.targetDbfs - rmsDb;
    if (desired > cfg_.maxGainDb) desired = cfg_.maxGainDb;
    if (desired < cfg_.minGainDb) desired = cfg_.minGainDb;
    const float alpha = desired < gainDb_ ? cfg_.attackAlpha : cfg_.releaseAlpha;
    gainDb_ += (desired - gainDb_) * alpha;
  } else {
    // Solo rumore: non lasciare il guadagno alto di prima (amplificherebbe il fondo come fosse voce).
    gainDb_ += (cfg_.idleGainDb - gainDb_) * cfg_.idleAlpha;
  }
  // Rampa lineare del guadagno nel frame, per evitare scalini udibili.
  const float g0 = std::pow(10.0f, before / 20.0f);
  const float g1 = std::pow(10.0f, gainDb_ / 20.0f);
  for (size_t i = 0; i < n; i++) {
    const float g = g0 + (g1 - g0) * (static_cast<float>(i + 1) / static_cast<float>(n));
    x[i] *= g;
  }
}

AudioFrontEnd::AudioFrontEnd(const FrontEndConfig& cfg) : cfg_(cfg), agc_(cfg.agc) {
  hpf_.setHighpass(static_cast<float>(kMicRate), cfg_.hpfHz);
  memset(tmp_, 0, sizeof(tmp_));
  memset(dec_out_, 0, sizeof(dec_out_));
}

void AudioFrontEnd::reset() {
  hpf_.reset();
  dec_.reset();
  agc_.reset();
}

void AudioFrontEnd::processFrame(const int32_t* in, int16_t* out) {
  for (size_t i = 0; i < kMicSamplesPerFrame; i++) {
    const float x = static_cast<float>(in[i]) / 2147483648.0f;  // 32 bit con segno -> [-1, 1)
    tmp_[i] = hpf_.process(x);
  }
  dec_.process(tmp_, kMicSamplesPerFrame, dec_out_);

  if (cfg_.agcEnabled) {
    agc_.process(dec_out_, kFrameSamples);
  } else {
    const float g = std::pow(10.0f, cfg_.fixedGainDb / 20.0f);
    for (size_t i = 0; i < kFrameSamples; i++) dec_out_[i] *= g;
  }

  for (size_t i = 0; i < kFrameSamples; i++) {
    float v = dec_out_[i] * 32768.0f;
    if (v > 32767.0f) v = 32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    out[i] = static_cast<int16_t>(std::lrintf(v));
  }
}

}  // namespace lcr
