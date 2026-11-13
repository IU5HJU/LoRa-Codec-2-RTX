#include "peer_resolve.h"
#include "testing.h"

using namespace lcr;

int main() {
  // indirizzo in cifre: mai
  PeerResolveSchedule lit("192.168.1.50");
  CHECK(lit.literal() && !lit.due(0, true, false) && !lit.due(1000000, true, false));
  // nome: appena c'e' il Wi-Fi, non prima
  PeerResolveSchedule n("reflector.example.org");
  CHECK(!n.literal() && !n.due(0, false, false) && n.due(0, true, false));
  n.done(true, 1000);
  CHECK(n.resolvedOnce());
  // collegamento attivo: si aggiorna ogni 5 minuti
  CHECK(!n.due(1000 + 299999, true, true) && n.due(1000 + 300000, true, true));
  // collegamento non attivo: ogni 30 s (il reflector puo' aver cambiato indirizzo)
  CHECK(!n.due(1000 + 29999, true, false) && n.due(1000 + 30000, true, false));
  // senza Wi-Fi non si prova
  CHECK(!n.due(1000 + 400000, false, false));
  // dopo un errore si riprova ogni 10 s, con o senza collegamento
  PeerResolveSchedule f("reflector.example.org");
  f.done(false, 5000);
  CHECK(!f.resolvedOnce() && !f.due(5000 + 9999, true, true) && f.due(5000 + 10000, true, false));
  f.done(true, 20000);
  CHECK(f.resolvedOnce() && !f.due(20001, true, true));
  // l'orologio che gira intorno a 2^32 non confonde
  PeerResolveSchedule w("a.b");
  w.done(true, 0xFFFFFF00u);
  CHECK(!w.due(0xFFFFFF00u + 1000, true, true) && w.due(0xFFFFFF00u + 300000u, true, true));
  PeerResolveSchedule nul(nullptr);
  CHECK(!nul.literal());
  TEST_MAIN_END();
}
