// Misuratore di tono (algoritmo di Goertzel): ampiezza di una sola frequenza in un blocco di campioni.
// Serve alla prova dell'hardware audio: la scheda suona un tono e il microfono ne misura la presenza
// proprio a quella frequenza, cosi' il rumore a banda larga non conta. Con un blocco di N campioni a fs la
// misura e' esatta se freq * N / fs e' un numero intero (500/1000/2000 Hz, 640 campioni, 16 kHz: bin 20/40/80).
// Indipendente dalla scheda e testato sul PC (tests/test_tone.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

class ToneMeter {
 public:
  void begin(float freqHz, float sampleRateHz);
  // Un blocco di campioni a 32 bit con segno (fondo scala 2^31). Aggiorna il massimo dei blocchi.
  void addBlock(const int32_t* samples, size_t n);
  void addBlock(const float* samples, size_t n);  // campioni in [-1, 1)
  // Ampiezza (dBFS) del blocco con il tono piu' forte dall'ultimo begin(); -120 se non ci sono blocchi.
  float maxDb() const;
  uint32_t blocks() const { return blocks_; }

 private:
  float coeff_ = 0.0f;
  float maxAmp_ = 0.0f;
  uint32_t blocks_ = 0;
};

}  // namespace lcr
