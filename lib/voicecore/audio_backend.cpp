#include "audio_backend.h"

#include <cmath>
#include <cstring>

namespace lcr {

Interpolator2::Interpolator2() {
  designLowpass(taps_, kTaps, 3600.0f, static_cast<float>(kMicRate));
  for (size_t j = 0; j < kTaps; j++) taps_[j] *= 2.0f;  // compensa lo zero-stuffing
  reset();
}

void Interpolator2::reset() { memset(hist_, 0, sizeof(hist_)); }

void Interpolator2::process(const float* in, size_t n, float* out) {
  if (n > kFrameSamples) n = kFrameSamples;
  float buf[kTaps - 1 + 2 * kFrameSamples];
  memcpy(buf, hist_, sizeof(hist_));
  for (size_t i = 0; i < n; i++) {
    buf[kTaps - 1 + 2 * i] = in[i];
    buf[kTaps - 1 + 2 * i + 1] = 0.0f;
  }
  const size_t m = 2 * n;
  for (size_t k = 0; k < m; k++) {
    float acc = 0;
    const size_t base = k + (kTaps - 1);
    for (size_t j = 0; j < kTaps; j++) acc += taps_[j] * buf[base - j];
    out[k] = acc;
  }
  memcpy(hist_, buf + m, sizeof(hist_));
}

AudioBackEnd::AudioBackEnd(const BackEndConfig& cfg) : cfg_(cfg), volumeDb_(cfg.volumeDb), agc_(cfg.agc) {
  memset(buf_, 0, sizeof(buf_));
  memset(up_buf_, 0, sizeof(up_buf_));
}

void AudioBackEnd::setVolumeDb(float db) {
  if (db > 0.0f) db = 0.0f;
  if (db < -60.0f) db = -60.0f;
  volumeDb_ = db;
  muted_ = false;
}

void AudioBackEnd::setVolumeStep(int step) {
  if (step <= 0) {
    muted_ = true;
    return;
  }
  if (step > kVolumeSteps) step = kVolumeSteps;
  setVolumeDb(-3.0f * static_cast<float>(kVolumeSteps - step));
}

void AudioBackEnd::reset() {
  agc_.reset();
  up_.reset();
}

void AudioBackEnd::processLost(int16_t* out) { memset(out, 0, outputSamplesPerFrame() * sizeof(int16_t)); }

void AudioBackEnd::processFrame(const int16_t* in, int16_t* out) {
  double sqIn = 0;
  for (size_t i = 0; i < kFrameSamples; i++) {
    buf_[i] = static_cast<float>(in[i]) / 32768.0f;
    sqIn += static_cast<double>(buf_[i]) * buf_[i];
  }
  inDb_ = 10.0f * std::log10(static_cast<float>(sqIn / kFrameSamples) + 1e-12f);

  if (cfg_.agcEnabled) agc_.process(buf_, kFrameSamples);

  const float vol = muted_ ? 0.0f : std::pow(10.0f, volumeDb_ / 20.0f);
  for (size_t i = 0; i < kFrameSamples; i++) buf_[i] *= vol;

  const float* src = buf_;
  size_t n = kFrameSamples;
  if (cfg_.upsample2x) {
    up_.process(buf_, kFrameSamples, up_buf_);
    src = up_buf_;
    n = 2 * kFrameSamples;
  }

  // Limitatore morbido: identita' fino alla soglia, poi tanh verso maxPeak.
  const float thr = cfg_.limiterThreshold;
  const float span = cfg_.maxPeak - thr;
  double sqOut = 0;
  limited_ = false;
  for (size_t i = 0; i < n; i++) {
    float x = src[i];
    const float a = std::fabs(x);
    if (a > thr && span > 0.0f) {
      limited_ = true;
      const float y = thr + span * std::tanh((a - thr) / span);
      x = x < 0 ? -y : y;
    }
    float v = x * 32768.0f;
    if (v > 32767.0f) v = 32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    out[i] = static_cast<int16_t>(std::lrintf(v));
    sqOut += static_cast<double>(v) * v;
  }
  outDb_ = 10.0f * std::log10(static_cast<float>(sqOut / n) / (32768.0f * 32768.0f) + 1e-12f);
}

}  // namespace lcr
