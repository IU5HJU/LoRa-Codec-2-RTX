// Statistiche del livello del microfono per "Misura il fondo": istogramma a passi di 1 dB.
// Indipendente dalla scheda e testata sul PC (tests/test_level.cpp).
#pragma once

#include <cstdint>

namespace lcr {

class LevelStats {
 public:
  static constexpr int kMinDb = -120;
  static constexpr int kMaxDb = 0;

  void reset();
  void add(float db);
  uint32_t count() const { return count_; }
  int minDb() const { return count_ ? min_ : kMinDb; }
  int maxDb() const { return count_ ? max_ : kMinDb; }
  // Livello sotto cui sta la percentuale p (0-100) dei campioni, in dB interi. Il 90% e' robusto
  // contro un colpo o un rumore isolato durante la misura.
  int percentile(int p) const;

 private:
  uint32_t bins_[kMaxDb - kMinDb + 1] = {0};
  uint32_t count_ = 0;
  int min_ = kMaxDb;
  int max_ = kMinDb;
};

}  // namespace lcr
