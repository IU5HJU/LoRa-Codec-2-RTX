// Catena audio del microfono: I2S 32 bit @16 kHz -> int16 @8 kHz per Codec2.
//
//   campioni I2S (24 bit nei 32) -> float -> passa-alto -> decimazione 2x -> AGC -> int16
//
// Indipendente dalla scheda e testata sul PC (tests/test_audio.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.h"

namespace lcr {

constexpr size_t kMicRate = 16000;
constexpr size_t kCodecRate = 8000;
// 40 ms di microfono a 16 kHz -> un frame Codec2 (320 campioni a 8 kHz).
constexpr size_t kMicSamplesPerFrame = kFrameSamples * 2;

// Filtro biquad passa-alto del 2o ordine (Butterworth).
class Biquad {
 public:
  void setHighpass(float sampleRate, float cutoffHz);
  float process(float x);
  void reset();

 private:
  float b0_ = 1, b1_ = 0, b2_ = 0, a1_ = 0, a2_ = 0;
  float z1_ = 0, z2_ = 0;
};

// Decimazione per 2 con FIR passa-basso (31 tappe, taglio ~3,6 kHz a 16 kHz).
class Decimator2 {
 public:
  static constexpr size_t kTaps = 31;
  Decimator2();
  // n deve essere pari e <= kMicSamplesPerFrame. Scrive n/2 campioni.
  void process(const float* in, size_t n, float* out);
  void reset();

 private:
  float taps_[kTaps];
  float hist_[kTaps - 1];
};

struct AgcConfig {
  float targetDbfs = -24.0f;   // livello RMS desiderato del frame
  float maxGainDb = 36.0f;     // il microfono a distanza di parlato e' intorno a -60 dBFS
  float minGainDb = -20.0f;
  float gateDbfs = -70.0f;     // sotto questo livello (rumore) il guadagno non sale
  float initialGainDb = 12.0f;
  float attackAlpha = 0.6f;    // riduzione del guadagno (segnale forte): veloce
  float releaseAlpha = 0.15f;  // aumento del guadagno: piu' lento
};

class Agc {
 public:
  explicit Agc(const AgcConfig& cfg = AgcConfig()) : cfg_(cfg), gainDb_(cfg.initialGainDb) {}
  // Normalizza il frame (campioni float in [-1, 1]) con guadagno interpolato.
  void process(float* x, size_t n);
  float gainDb() const { return gainDb_; }
  void reset() { gainDb_ = cfg_.initialGainDb; }

 private:
  AgcConfig cfg_;
  float gainDb_;
};

struct FrontEndConfig {
  float hpfHz = 300.0f;
  bool agcEnabled = true;
  float fixedGainDb = 24.0f;  // usato se l'AGC e' disattivato
  AgcConfig agc;
};

class AudioFrontEnd {
 public:
  explicit AudioFrontEnd(const FrontEndConfig& cfg = FrontEndConfig());
  // in: kMicSamplesPerFrame parole I2S a 32 bit (24 bit significativi, allineati a sinistra).
  // out: kFrameSamples campioni int16 a 8 kHz.
  void processFrame(const int32_t* in, int16_t* out);
  float gainDb() const { return cfg_.agcEnabled ? agc_.gainDb() : cfg_.fixedGainDb; }
  void reset();

 private:
  FrontEndConfig cfg_;
  Biquad hpf_;
  Decimator2 dec_;
  Agc agc_;
  float tmp_[kMicSamplesPerFrame];
  float dec_out_[kFrameSamples];
};

}  // namespace lcr
