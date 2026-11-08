// Quando spegnere l'amplificatore dopo l'ultimo suono.
//
// L'ora "adesso" di un giro del loop e' presa all'inizio del giro, ma durante il giro la riproduzione aggiorna
// l'istante dell'ultimo suono con un valore piu' recente: la differenza tra i due e' allora negativa. Con numeri senza
// segno diventerebbe enorme e l'amplificatore verrebbe spento subito dopo ogni frame (voce a pezzi e scatti).
// Per questo si confronta con il segno e un ultimo suono "nel futuro" non e' mai inattivita'.
#pragma once

#include <cstdint>

namespace lcr {

inline bool ampIdleLongerThan(uint32_t nowMs, uint32_t lastSoundMs, uint32_t holdMs) {
  return static_cast<int32_t>(nowMs - lastSoundMs) > static_cast<int32_t>(holdMs);
}

}  // namespace lcr
