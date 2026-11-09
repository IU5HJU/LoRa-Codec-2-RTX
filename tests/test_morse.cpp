#include <cstring>

#include "morse.h"
#include "testing.h"

using namespace lcr;

static bool is(const MorseElement& e, bool tone, uint16_t ms) { return e.tone == tone && e.ms == ms; }

static void testLetters() {
  MorseElement e[32];
  // S = tre punti: punto, pausa, punto, pausa, punto
  size_t n = morseElements("S", 10, e, 32);
  CHECK(n == 5);
  CHECK(is(e[0], true, 10) && is(e[1], false, 10) && is(e[2], true, 10) && is(e[3], false, 10) && is(e[4], true, 10));
  // M = due linee
  n = morseElements("M", 10, e, 32);
  CHECK(n == 3 && is(e[0], true, 30) && is(e[1], false, 10) && is(e[2], true, 30));
  // G = linea, linea, punto
  n = morseElements("g", 10, e, 32);  // minuscole come maiuscole
  CHECK(n == 5 && is(e[0], true, 30) && is(e[2], true, 30) && is(e[4], true, 10));
  // cifre
  n = morseElements("5", 10, e, 32);
  CHECK(n == 9);  // cinque punti e quattro pause
}

static void testMessageTone() {
  MorseElement e[48];
  const size_t n = morseElements(kMessageTone, kMessageToneUnitMs, e, 48);
  // M (3 elementi) + pausa + S (5) + pausa + G (5) = 15
  CHECK(n == 15);
  CHECK(is(e[3], false, 3u * kMessageToneUnitMs));  // pausa tra le lettere: 3 unita'
  CHECK(is(e[9], false, 3u * kMessageToneUnitMs));
  // Durata: M 7 + 3 + S 5 + 3 + G 9 = 27 unita'
  CHECK(morseDurationMs(e, n) == 27u * kMessageToneUnitMs);
  // La durata dei toni singoli resta sotto il limite di 600 ms di beep().
  for (size_t i = 0; i < n; i++) CHECK(e[i].ms <= 600);
  // Il tono inizia e finisce con un suono (nessuna pausa ai bordi).
  CHECK(e[0].tone && e[n - 1].tone);
}

static void testWordsAndUnknown() {
  MorseElement e[32];
  // Due lettere separate da spazio: pausa di 7 unita'.
  size_t n = morseElements("E E", 10, e, 32);
  CHECK(n == 3 && is(e[0], true, 10) && is(e[1], false, 70) && is(e[2], true, 10));
  // Spazi iniziali e finali, caratteri sconosciuti: ignorati.
  n = morseElements("  E?#E ", 10, e, 32);
  CHECK(n == 3 && is(e[1], false, 30));
  CHECK(morseElements("", 10, e, 32) == 0);
  CHECK(morseElements(nullptr, 10, e, 32) == 0);
  // Il vettore pieno ferma la scrittura senza uscire dai limiti.
  CHECK(morseElements("MSG", 10, e, 4) == 4);
  CHECK(morseDurationMs(e, 0) == 0);
}

int main() {
  testLetters();
  testMessageTone();
  testWordsAndUnknown();
  TEST_MAIN_END();
}
