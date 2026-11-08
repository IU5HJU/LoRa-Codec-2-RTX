// Spegnimento dell'amplificatore: l'ultimo suono piu' recente dell'ora del giro non deve spegnerlo.
#include "amp_hold.h"
#include "testing.h"

using namespace lcr;

int main() {
  constexpr uint32_t kHold = 400;
  CHECK(!ampIdleLongerThan(1000, 1000, kHold));          // appena suonato
  CHECK(!ampIdleLongerThan(1000, 700, kHold));           // 300 ms fa: ancora acceso
  CHECK(!ampIdleLongerThan(1400, 1000, kHold));          // esattamente 400 ms: non oltre
  CHECK(ampIdleLongerThan(1401, 1000, kHold));           // oltre 400 ms: si spegne
  // Il caso del difetto: l'ora del giro e' piu' vecchia dell'ultimo suono (suonato durante il giro).
  CHECK(!ampIdleLongerThan(1000, 1013, kHold));          // 13 ms dopo (decodifica di un frame)
  CHECK(!ampIdleLongerThan(1000, 1040, kHold));          // 40 ms dopo (frame scritto con attesa)
  CHECK(!ampIdleLongerThan(1000, 1300, kHold));          // anche molto dopo
  // Giro di millis() a 32 bit.
  CHECK(!ampIdleLongerThan(5, 0xFFFFFFF0u, kHold));      // ha girato da 21 ms
  CHECK(ampIdleLongerThan(500, 0xFFFFFFF0u, kHold));     // ha girato da 516 ms
  CHECK(!ampIdleLongerThan(0xFFFFFFF0u, 5, kHold));      // ultimo suono "dopo" l'ora (giro di millis)
  TEST_MAIN_END();
}
