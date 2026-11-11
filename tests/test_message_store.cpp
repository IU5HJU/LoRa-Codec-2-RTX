// Archivio dei messaggi del ripetitore: ricezione intera, parita', limite di 15 s, sessioni, coda e scadenza.
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "message_store.h"
#include "packet.h"
#include "profile.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;

static std::vector<Packet> makeMessage(uint16_t session, int frames, bool total = true) {
  std::vector<Packet> v;
  TxSession tx("IU5HJU", session, true);
  if (total) tx.setExpectedTotal(TxSession::expectedPackets(frames, true));
  auto drain = [&]() {
    Packet p;
    while (tx.popPacket(p)) v.push_back(p);
  };
  tx.begin();
  drain();
  for (int i = 0; i < frames; i++) {
    uint8_t f[kFrameBytes];
    for (size_t k = 0; k < kFrameBytes; k++) f[k] = static_cast<uint8_t>(i * 7 + k * 31 + 1);
    tx.pushFrame(f);
    drain();
  }
  tx.finish();
  drain();
  return v;
}

typedef MessageStore::Result R;

static R feedAll(MessageStore& st, const std::vector<Packet>& v, const std::vector<size_t>& skip, uint32_t& t) {
  R last = R::Accepted;
  bool done = false;
  for (size_t i = 0; i < v.size(); i++) {
    t += 3400;
    bool dropIt = false;
    for (size_t s : skip) dropIt = dropIt || s == i;
    if (dropIt) continue;
    last = st.onPacket(v[i], t);
    if (last == R::Complete) done = true;
  }
  return done ? R::Complete : last;
}

static bool samePayload(const Packet& a, const Packet& b) { return a.payloadLen == b.payloadLen && memcmp(a.payload, b.payload, a.payloadLen) == 0; }

static void checkReadyMatches(const MessageStore& st, const std::vector<Packet>& orig, uint16_t newSession) {
  CHECK(st.readyPackets() == orig.size());
  for (size_t i = 0; i < orig.size(); i++) {
    Packet out;
    CHECK(st.readyPacket(i, newSession, out));
    CHECK(out.seq == orig[i].seq && out.mode == orig[i].mode && out.session == newSession);
    CHECK((out.flags & kFlagRelayed) != 0);
    CHECK((out.flags & ~kFlagRelayed & ~kFlagTotal & ~kFlagCallsign) == (orig[i].flags & ~kFlagTotal & ~kFlagCallsign));
    CHECK(samePayload(out, orig[i]));  // dati e parita' identici a quelli dell'originale
  }
}

static void testComplete() {
  std::unique_ptr<MessageStore> sp(new MessageStore);
  MessageStore& st = *sp;
  const auto msg = makeMessage(0x1111, 100);  // 4 s di voce
  uint32_t t = 0;
  CHECK(feedAll(st, msg, {}, t) == R::Complete);
  CHECK(st.readyCount() == 1 && !st.receiving());
  checkReadyMatches(st, msg, 0x2222);
  Packet start;
  CHECK(st.readyPacket(0, 7, start) && (start.flags & kFlagStart) && strcmp(start.callsign, "IU5HJU") == 0 && start.totalPackets == msg.size());
  CHECK(st.stats().completed == 1);
}

static void testRecoverDataAndParity() {
  std::unique_ptr<MessageStore> sp(new MessageStore);
  MessageStore& st = *sp;
  const auto msg = makeMessage(0x2222, 100);
  uint32_t t = 0;
  // pacchetto dati 2 perso (si ricostruisce con la parita. 4) e parita. 9 persa (si rigenera)
  CHECK(feedAll(st, msg, {2, 9}, t) == R::Complete);
  CHECK(st.stats().recovered >= 1);
  checkReadyMatches(st, msg, 0x3333);
  // Dati 1 e 2 persi nello stesso gruppo: non recuperabili, il messaggio non si completa e dopo il silenzio si abbandona
  MessageStore st2;
  t = 0;
  CHECK(feedAll(st2, msg, {1, 2}, t) != R::Complete);
  CHECK(st2.receiving() && st2.readyCount() == 0);
  st2.tick(t + MessageStore::kSilenceMs + 1);
  CHECK(!st2.receiving() && st2.readyCount() == 0 && st2.stats().abandoned == 1);
}

static void testStartLost() {
  std::unique_ptr<MessageStore> sp(new MessageStore);
  MessageStore& st = *sp;
  const auto msg = makeMessage(0x3333, 100);
  uint32_t t = 0;
  // Lo START (seq 0) e' ricostruito dalla parita': il nominativo si prende dall'END e il totale si ricalcola
  CHECK(feedAll(st, msg, {0}, t) == R::Complete);
  Packet start;
  CHECK(st.readyPacket(0, 9, start) && (start.flags & kFlagStart));
  CHECK(strcmp(start.callsign, "IU5HJU") == 0 && start.totalPackets == msg.size());
  Packet out;
  for (size_t i = 1; i < msg.size(); i++) {
    CHECK(st.readyPacket(i, 9, out) && samePayload(out, msg[i]));
  }
}

static void testTooLong() {
  std::unique_ptr<MessageStore> sp(new MessageStore);
  MessageStore& st = *sp;
  uint32_t t = 0;
  // 15 s esatti (375 frame): accettato
  const auto ok = makeMessage(0x4444, 375);
  CHECK(ok.size() == 42 && feedAll(st, ok, {}, t) == R::Complete);
  st.popReady();
  // 16 s (400 frame): rifiutato gia' dallo START, nulla in coda
  const auto bad = makeMessage(0x5555, 400);
  CHECK(bad.size() > 42);
  CHECK(st.onPacket(bad[0], t += 100) == R::TooLong);
  CHECK(!st.receiving());
  uint32_t t2 = t;
  CHECK(feedAll(st, bad, {}, t2) != R::Complete);
  st.tick(t2 + 100000);
  CHECK(st.readyCount() == 0 && st.stats().tooLong >= 1);
  // Senza TOTAL (START perso) il limite si ricava dall'END
  std::unique_ptr<MessageStore> sp3(new MessageStore);
  const auto bad2 = makeMessage(0x6666, 400, false);
  t = 0;
  CHECK(feedAll(*sp3, bad2, {0}, t) != R::Complete && sp3->readyCount() == 0);
}

static void testFilters() {
  std::unique_ptr<MessageStore> sp(new MessageStore);
  MessageStore& st = *sp;
  auto msg = makeMessage(0x7777, 100);
  Packet p = msg[1];
  p.mode = kModeCodec2_3200;
  CHECK(st.onPacket(p, 0) == R::Ignored);          // altro modo
  p = msg[1];
  p.flags |= kFlagRelayed;
  CHECK(st.onPacket(p, 0) == R::Ignored);          // gia' ripetuto
  CHECK(st.onPacket(msg[0], 10) == R::Accepted);
  CHECK(st.onPacket(msg[0], 20) == R::Duplicate);
  // Un'altra sessione mentre se ne riceve una: ignorata. Dopo il silenzio ricomincia.
  const auto other = makeMessage(0x8888, 100);
  CHECK(st.onPacket(other[0], 30) == R::Ignored);
  CHECK(st.onPacket(other[0], 30 + MessageStore::kSilenceMs + 5) == R::Accepted);
  CHECK(st.stats().abandoned == 1);
}

static void testQueueAndExpiry() {
  std::unique_ptr<MessageStore> sp(new MessageStore);
  MessageStore& st = *sp;
  uint32_t t = 0;
  for (int i = 0; i < 4; i++) {
    const auto msg = makeMessage(static_cast<uint16_t>(100 + i), 50);
    CHECK(feedAll(st, msg, {}, t) == R::Complete);
    t += 1000;
  }
  CHECK(st.readyCount() == MessageStore::kQueue && st.stats().droppedOld == 1);  // il piu' vecchio si e' perso
  // In testa c'e' il secondo messaggio inviato (la sessione originale non si conserva: si riconosce dal nominativo e dalla durata)
  st.popReady();
  CHECK(st.readyCount() == 2);
  // Scadenza dopo 30 minuti
  st.tick(t + MessageStore::kExpireMs + 1);
  CHECK(st.readyCount() == 0);
}

static void testAirtime() {
  std::unique_ptr<MessageStore> sp(new MessageStore);
  MessageStore& st = *sp;
  const auto msg = makeMessage(0x9999, 375);  // 15 s
  uint32_t t = 0;
  CHECK(feedAll(st, msg, {}, t) == R::Complete);
  const Profile& m = profileFor(kProfileMessage);
  const double air = st.readyAirtimeMs(m) / 1000.0;
  CHECK(air > 125 && air < 160);  // circa 139 s
  const double model = messageAirtimeMs(15000, 6, m) / 1000.0;
  CHECK(air > model * 0.95 && air < model * 1.05);
  CHECK(MessageStore().readyAirtimeMs(m) == 0);
}

int main() {
  testComplete();
  testRecoverDataAndParity();
  testStartLost();
  testTooLong();
  testFilters();
  testQueueAndExpiry();
  testAirtime();
  TEST_MAIN_END();
}
