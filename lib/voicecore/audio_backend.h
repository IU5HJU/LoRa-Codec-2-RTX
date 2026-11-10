// Blocco di uscita audio: frame decodificato da Codec2 -> altoparlante.
//
//   int16 @8 kHz -> AGC (opzionale) -> volume -> limitatore -> [raddoppio a 16 kHz] -> int16
//
// L'AGC riporta stazioni diverse allo stesso livello, il volume e' quello
// dell'utente e il limitatore tiene il picco sotto una soglia per non
// sovraccaricare amplificatore e altoparlante. Testato sul PC.
#pragma once

#include <cstddef>
#include <cstdint>

#include "audio_frontend.h"
#include "protocol.h"

namespace lcr {

// Raddoppio della frequenza di campionamento 8 -> 16 kHz (FIR a 31 tappe).
class Interpolator2 {
 public:
  static constexpr size_t kTaps = 31;
  Interpolator2();
  // in: n campioni; out: 2*n campioni. n <= kFrameSamples.
  void process(const float* in, size_t n, float* out);
  void reset();

 private:
  float taps_[kTaps];
  float hist_[kTaps - 1];
};

struct BackEndConfig {
  bool agcEnabled = true;
  AgcConfig agc = makeAgc();
  float volumeDb = -6.0f;          // volume iniziale
  float limiterThreshold = 0.6f;   // frazione del fondo scala da cui il limitatore interviene
  float maxPeak = 0.9f;            // il picco in uscita non supera questa frazione
  bool upsample2x = false;         // true: uscita a 16 kHz (640 campioni per frame)

  static AgcConfig makeAgc() {
    AgcConfig c;
    c.targetDbfs = -24.0f;
    c.maxGainDb = 24.0f;
    c.minGainDb = -20.0f;
    c.gateDbfs = -60.0f;  // il parlato decodificato non scende sotto questo livello
    c.initialGainDb = 0.0f;
    return c;
  }
};

class AudioBackEnd {
 public:
  // Volume: 0 = muto; 1..10 in passi da 3 dB. Il passo 5 e' 0 dB (il vecchio massimo); i passi 6..10 danno guadagno digitale
  // (fino a +15 dB), con il limitatore morbido che tiene i picchi sotto il fondo scala. Passo 1 = -12 dB.
  static constexpr int kVolumeSteps = 10;
  static constexpr int kUnityStep = 5;
  static constexpr float kStepDb = 3.0f;
  static constexpr float kMaxVolumeDb = kStepDb * (kVolumeSteps - kUnityStep);  // +15 dB

  explicit AudioBackEnd(const BackEndConfig& cfg = BackEndConfig());

  // Campioni scritti per ogni frame: 320 a 8 kHz, 640 con upsample2x.
  size_t outputSamplesPerFrame() const { return cfg_.upsample2x ? 2 * kFrameSamples : kFrameSamples; }

  void setVolumeDb(float db);
  void setVolumeStep(int step);  // 0..kVolumeSteps, passi da 3 dB
  float volumeDb() const { return volumeDb_; }
  bool muted() const { return muted_; }

  void processFrame(const int16_t* in, int16_t* out);
  // Livelli dell'ultimo frame (dBFS, RMS): decodificato prima dell'AGC e in uscita dopo AGC, volume e limitatore.
  float inputDb() const { return inDb_; }
  float outputDb() const { return outDb_; }
  float agcGainDb() const { return agc_.gainDb(); }
  bool limited() const { return limited_; }  // il limitatore e' intervenuto nell'ultimo frame
  // Frame perso e non recuperabile: silenzio, senza toccare AGC e filtri.
  void processLost(int16_t* out);
  void reset();

 private:
  BackEndConfig cfg_;
  float volumeDb_;
  bool muted_ = false;
  Agc agc_;
  Interpolator2 up_;
  float inDb_ = -120.0f;
  float outDb_ = -120.0f;
  bool limited_ = false;
  float buf_[kFrameSamples];
  float up_buf_[2 * kFrameSamples];
};

}  // namespace lcr
