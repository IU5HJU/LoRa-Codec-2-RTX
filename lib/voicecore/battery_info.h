// Indicazione della batteria sul display: testo, soglia di batteria scarica e media delle
// letture dell'ADC. Indipendente dalla scheda e testato sul PC (tests/test_battery.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

// Sotto questa tensione (V) la batteria e' considerata scarica (il testo va in negativo).
constexpr float kLowBatteryVolts = 3.40f;

// Scrive "(Batt=4.12V)" oppure "(Batt=--)" se la tensione non e' disponibile (volts < 0),
// come nel firmware di Guzman. Ritorna la lunghezza del testo.
size_t formatBatteryText(char* out, size_t cap, float volts);

bool isLowBattery(float volts);

// Una LiPo/Li-ion non supera 4,2 V (4,35 V le celle ad alta tensione). Sulla T3 senza batteria, con l'USB
// collegato, l'ADC legge la tensione del caricatore (misurati 4,14-4,42 V, che oscilla): una lettura sopra
// questo limite non e' una batteria.
constexpr float kMaxBatteryVolts = 4.35f;
// Dopo una lettura sopra il limite la tensione resta "non disponibile" per almeno questo tempo: se il
// caricatore oscilla attorno al limite il display non deve passare di continuo da tensione a "--".
constexpr uint32_t kBatteryHoldMs = 60000;

class BatteryGuard {
 public:
  // volts se plausibile, altrimenti -1 (non disponibile). Valori negativi restano -1.
  float filter(float volts, uint32_t nowMs);

 private:
  bool seen_ = false;
  uint32_t lastBadMs_ = 0;
};

// Media mobile delle ultime N letture, per smussare il rumore dell'ADC senza bloccare il ciclo
// principale (Guzman prende 20 letture in 60 ms con delay: qui una lettura ogni tanto).
class AverageFilter {
 public:
  static constexpr size_t kWindow = 16;
  void reset() {
    count_ = 0;
    next_ = 0;
  }
  void add(float v) {
    buf_[next_] = v;
    next_ = (next_ + 1) % kWindow;
    if (count_ < kWindow) count_++;
  }
  size_t count() const { return count_; }
  // Media delle letture presenti; 0 se non ce ne sono.
  float average() const {
    if (count_ == 0) return 0.0f;
    float sum = 0;
    for (size_t i = 0; i < count_; i++) sum += buf_[i];
    return sum / static_cast<float>(count_);
  }

 private:
  float buf_[kWindow] = {0};
  size_t count_ = 0;
  size_t next_ = 0;
};

}  // namespace lcr
