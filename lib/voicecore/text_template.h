// Segnaposti nei testi da inviare, sostituiti al momento dell'invio:
//   %C   il tuo nominativo (IU5HJU)
//   %M   la lettera del modo del profilo voce in uso: S Standard, M Messaggio, Q Qualita'
//   %F   la frequenza del profilo voce in MHz (433.900)
//   %%   il segno %
// Per esempio "%C QRV su VoLora modo %M %F" diventa "IU5HJU QRV su VoLora modo Q 433.900". Un altro carattere dopo % resta com'e'.
// Indipendente dalla scheda e testato sul PC (tests/test_text_template.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

// Scrive in 'out' (cap byte, terminato) il testo con i segnaposti sostituiti. Ritorna la lunghezza, oppure 0 se il risultato e'
// vuoto, piu' lungo di kMaxTextChars o non e' un testo valido (isValidText).
size_t expandTextTemplate(const char* tmpl, const char* callsign, char modeLetter, uint32_t freqKhz, char* out, size_t cap);

}  // namespace lcr
