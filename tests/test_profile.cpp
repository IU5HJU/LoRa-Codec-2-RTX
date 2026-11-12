// Profili di trasmissione: tabella, nomi e tempi in aria.
#include <cmath>
#include <cstring>

#include "profile.h"
#include "testing.h"

using namespace lcr;

static void testTable() {
  CHECK(profileFor(kProfileRange).sf == 8 && profileFor(kProfileRange).bwKhz == 250.0f && !profileFor(kProfileRange).message);
  CHECK(profileFor(kProfileMessage).sf == 12 && profileFor(kProfileMessage).bwKhz == 125.0f && profileFor(kProfileMessage).message);
  CHECK(profileFor(200).id == kProfileStandard);  // id sconosciuto: Standard (dal 12/11/2026 il Codec2 3200)
  // Il Range e' il vecchio Standard: i valori storici
  const Profile& s = profileFor(kProfileRange);
  CHECK(strcmp(s.code, "rng") == 0 && strcmp(s.name, "Range") == 0 && s.codecMode == kModeCodec2_1200);
  CHECK(s.maxTurnMs == kMaxTurnMs && s.rxPrebufferFrames == kPrebufferFrames && s.rxSessionTimeoutMs == kSessionTimeoutMs);
  const Profile& q = profileFor(kProfileStandard);
  CHECK(q.sf == 7 && q.bwKhz == 250.0f && !q.message && q.codecMode == kModeCodec2_3200);
  CHECK(strcmp(q.code, "std") == 0 && strcmp(q.name, "Standard") == 0);
  CHECK(frameBytesFor(q.codecMode) == 16 && frameBytesFor(kModeCodec2_1200) == 6);
  // gli id non cambiano: 0 e' sempre il Codec2 1200, 2 il Codec2 3200 (le impostazioni gia' salvate restano valide)
  CHECK(kProfileRange == 0 && kProfileMessage == 1 && kProfileStandard == 2);
  uint8_t id = 9;
  CHECK(profileFromCode("qua", id) && id == kProfileStandard);  // il vecchio nome del 3200 vale ancora
  CHECK(profileFromCode("QUA", id) && id == kProfileStandard && !profileFromCode("quas", id) && !profileFromCode("qu", id));
  CHECK(profileFromCode("rng", id) && id == kProfileRange && profileFromCode("RNG", id) && id == kProfileRange);
  CHECK(profileFromCode("msg", id) && id == kProfileMessage);
  CHECK(profileFromCode("MSG", id) && id == kProfileMessage);
  CHECK(profileFromCode("Std", id) && id == kProfileStandard);
  CHECK(!profileFromCode("mess", id) && !profileFromCode("", id) && !profileFromCode(nullptr, id));
}

static void testMeshcom() {
  // Escursione sulle reti di testo: non e' un profilo selezionabile
  const Profile& p = meshcomProfile();
  CHECK(strcmp(p.code, "mc") == 0 && strcmp(p.name, "MeshCom") == 0 && p.textOnly && !p.message);
  CHECK(p.sf == 11 && p.bwKhz == 250.0f && p.cr == 6 && p.preambleSymbols == 8 && p.syncWord == 0x2B);
  CHECK(p.maxTurnMs == 0);  // niente voce
  uint8_t id = 0;
  CHECK(!profileFromCode("mc", id) && kProfileCount == 3);
  // i profili veri non sono di solo testo e usano la sync word di user_config.h
  for (uint8_t i = 0; i < kProfileCount; i++) CHECK(!profileFor(i).textOnly && profileFor(i).syncWord == 0);
  // Confronto con i tempi in aria misurati da un nodo vero (CHANNEL_UTIL: 43 byte 566 ms, 58 byte 711 ms, 106 byte 1154 ms)
  CHECK(std::fabs(airtimeMs(43, p) - 566) < 15 && std::fabs(airtimeMs(58, p) - 711) < 15 && std::fabs(airtimeMs(106, p) - 1154) < 15);
  CHECK(airtimeMs(100, p) > airtimeMs(60, p));
}

static void testAirtime() {
  // Valori di riferimento calcolati con la formula di Semtech (docs/roadmap.md)
  const Profile& s = profileFor(kProfileRange);
  CHECK(std::fabs(airtimeMs(77, s) - 127.2) < 0.5);   // pacchetto dati senza nominativo
  CHECK(std::fabs(airtimeMs(84, s) - 137.5) < 0.5);   // con nominativo di 6 caratteri
  CHECK(std::fabs(airtimeMs(80, s) - 132.4) < 0.5);   // parita'
  const Profile& m = profileFor(kProfileMessage);
  CHECK(airtimeMs(77, m) > 3300 && airtimeMs(77, m) < 3500);  // SF12/BW125: circa 3,4 s
  CHECK(airtimeMs(80, m) > airtimeMs(77, m) - 1);             // piu' byte non durano meno
}

static void testMessageBudget() {
  const Profile& m = profileFor(kProfileMessage);
  CHECK(m.maxTurnMs == 15000 && m.maxTurnMs == kMessageMaxTurnMs);  // 11 novembre: 15 s, quanto ripete il ripetitore (prima 30 s)
  CHECK(profileFor(kProfileRange).maxTurnMs == 30000 && profileFor(kProfileStandard).maxTurnMs == 30000);  // gli altri restano a 30 s
  const double air = messageAirtimeMs(m.maxTurnMs, 6, m);
  // Il messaggio piu' lungo (15 s di voce a 1200 bps) occupa circa 139 s in aria: e' il tetto, non il budget
  CHECK(air > 125000 && air < 155000);
  CHECK(messageAirtimeMs(10000, 6, m) > 85000 && messageAirtimeMs(10000, 6, m) < 100000);
  // Un messaggio corto costa poco
  CHECK(messageAirtimeMs(3000, 6, m) < 40000);
  // Qualita': 30 s a 3200 bps in circa 12,5 s in aria (42 %), entro il budget del 70 %
  const Profile& q = profileFor(kProfileStandard);
  const double qa = messageAirtimeMs(q.maxTurnMs, 6, q);
  CHECK(qa > 11000 && qa < 14500);
  CHECK(qa < 0.70 * q.maxTurnMs);
  // Lo Standard resta entro il 70 % del tempo di voce (budget del 5 novembre), parita' compresa, senza pause
  const Profile& s = profileFor(kProfileRange);
  CHECK(messageAirtimeMs(s.maxTurnMs, 6, s) < 0.70 * s.maxTurnMs);
}

int main() {
  testTable();
  testMeshcom();
  testAirtime();
  testMessageBudget();
  TEST_MAIN_END();
}
