// Profili di trasmissione: tabella, nomi e tempi in aria.
#include <cmath>

#include "profile.h"
#include "testing.h"

using namespace lcr;

static void testTable() {
  CHECK(profileFor(kProfileStandard).sf == 8 && profileFor(kProfileStandard).bwKhz == 250.0f && !profileFor(kProfileStandard).message);
  CHECK(profileFor(kProfileMessage).sf == 12 && profileFor(kProfileMessage).bwKhz == 125.0f && profileFor(kProfileMessage).message);
  CHECK(profileFor(200).id == kProfileStandard);  // id sconosciuto: Standard
  // Lo Standard e' quello di sempre
  const Profile& s = profileFor(kProfileStandard);
  CHECK(s.maxTurnMs == kMaxTurnMs && s.rxPrebufferFrames == kPrebufferFrames && s.rxSessionTimeoutMs == kSessionTimeoutMs);
  uint8_t id = 9;
  CHECK(profileFromCode("msg", id) && id == kProfileMessage);
  CHECK(profileFromCode("MSG", id) && id == kProfileMessage);
  CHECK(profileFromCode("Std", id) && id == kProfileStandard);
  CHECK(!profileFromCode("mess", id) && !profileFromCode("", id) && !profileFromCode(nullptr, id));
}

static void testAirtime() {
  // Valori di riferimento calcolati con la formula di Semtech (docs/roadmap.md)
  const Profile& s = profileFor(kProfileStandard);
  CHECK(std::fabs(airtimeMs(77, s) - 127.2) < 0.5);   // pacchetto dati senza nominativo
  CHECK(std::fabs(airtimeMs(84, s) - 137.5) < 0.5);   // con nominativo di 6 caratteri
  CHECK(std::fabs(airtimeMs(80, s) - 132.4) < 0.5);   // parita'
  const Profile& m = profileFor(kProfileMessage);
  CHECK(airtimeMs(77, m) > 3300 && airtimeMs(77, m) < 3500);  // SF12/BW125: circa 3,4 s
  CHECK(airtimeMs(80, m) > airtimeMs(77, m) - 1);             // piu' byte non durano meno
}

static void testMessageBudget() {
  const Profile& m = profileFor(kProfileMessage);
  const double air = messageAirtimeMs(m.maxTurnMs, 6, m);
  // Il messaggio piu' lungo (10 s di voce a 1200 bps) sta in meno di 100 s in aria: budget del profilo Messaggio
  CHECK(air > 80000 && air < 100000);
  // Un messaggio corto costa poco
  CHECK(messageAirtimeMs(3000, 6, m) < 40000);
  // Lo Standard resta entro il 70 % del tempo di voce (budget del 5 novembre), parita' compresa, senza pause
  const Profile& s = profileFor(kProfileStandard);
  CHECK(messageAirtimeMs(s.maxTurnMs, 6, s) < 0.70 * s.maxTurnMs);
}

int main() {
  testTable();
  testAirtime();
  testMessageBudget();
  TEST_MAIN_END();
}
