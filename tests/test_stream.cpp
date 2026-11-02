#include <cstring>
#include <set>
#include <vector>

#include "rx_stream.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;

static void frameFor(int i, uint8_t* f) {
  f[0] = static_cast<uint8_t>(i & 0xFF);
  f[1] = static_cast<uint8_t>((i >> 8) & 0xFF);
  memset(f + 2, 0x33, 4);
}
static int frameId(const uint8_t* f) { return f[0] | (f[1] << 8); }

// Produce tutti i pacchetti di una trasmissione di nFrames frame.
static std::vector<Packet> makeTx(int nFrames, bool fec = true) {
  TxSession tx("IU5HJU", 0x1234, fec);
  std::vector<Packet> out;
  tx.begin();
  for (int i = 1; i <= nFrames; i++) {
    uint8_t f[kFrameBytes];
    frameFor(i, f);
    CHECK(tx.pushFrame(f));
    Packet p;
    while (tx.popPacket(p)) out.push_back(p);
  }
  tx.finish();
  Packet p;
  while (tx.popPacket(p)) out.push_back(p);
  return out;
}

// Consegna i pacchetti (tranne quelli in 'drop', per indice di invio) e
// riproduce tutto; ritorna gli id dei frame, -1 per i frame persi.
static std::vector<int> receive(const std::vector<Packet>& pk, const std::set<int>& drop, RxStream* rxOut = nullptr) {
  RxStream rx;
  uint32_t now = 0;
  for (size_t i = 0; i < pk.size(); i++) {
    if (drop.count(static_cast<int>(i))) continue;
    // Il pacchetto passa dal formato su cavo, come in radio.
    uint8_t buf[kMaxWireBytes];
    const size_t n = encodePacket(pk[i], buf, sizeof(buf));
    Packet q;
    CHECK(n > 0 && decodePacket(buf, n, q));
    CHECK(rx.onPacket(q, now));
    now += 100;
  }
  std::vector<int> got;
  for (int guard = 0; guard < 5000; guard++, now += kFrameMs) {
    RxStream::Out o = rx.nextFrame(now);
    if (o.kind == RxStream::Kind::End) break;
    if (o.kind == RxStream::Kind::Frame) got.push_back(frameId(o.frame));
    if (o.kind == RxStream::Kind::Lost) got.push_back(-1);
  }
  if (rxOut) *rxOut = rx;
  return got;
}

static std::vector<int> expectAll(int n) {
  std::vector<int> v;
  for (int i = 1; i <= n; i++) v.push_back(i);
  return v;
}

static void testTxLayout() {
  const auto pk = makeTx(75);  // 75 frame = 6 pacchetti pieni + 3 frame
  // seq: 0 START, 1-3 dati, 4 parita', 5-7 dati, 8 END, 9 parita'
  CHECK(pk.size() == 10);
  for (size_t i = 0; i < pk.size(); i++) CHECK(pk[i].seq == i);
  CHECK((pk[0].flags & kFlagStart) && pk[0].payloadLen == 0 && strcmp(pk[0].callsign, "IU5HJU") == 0);
  CHECK(pk[1].payloadLen == 72 && !pk[1].hasCallsign());
  CHECK(pk[4].isParity() && pk[9].isParity());
  CHECK((pk[8].flags & kFlagEnd) && pk[8].payloadLen == 18 && strcmp(pk[8].callsign, "IU5HJU") == 0);
  CHECK(pk[9].payload[0] == 4);
  for (const auto& p : pk) CHECK(p.session == 0x1234);
}

static void testTxFrameBoundaryAndNoFec() {
  // Esattamente 12 frame: l'END e' vuoto.
  const auto pk = makeTx(12);
  CHECK(pk.size() == 4);  // START, dati, END vuoto, parita' (count 3)
  CHECK((pk[2].flags & kFlagEnd) && pk[2].payloadLen == 0);
  CHECK(pk[3].isParity() && pk[3].payload[0] == 3);

  const auto nofec = makeTx(75, false);
  for (const auto& p : nofec) {
    CHECK(!p.isParity());
    CHECK(p.flags & kFlagNoFec);
  }
  CHECK(nofec.size() == 8);
  CHECK(receive(nofec, {}) == expectAll(75));
}

static void testCallsignRepeat() {
  const auto pk = makeTx(750);  // 30 s
  int withCs = 0;
  for (const auto& p : pk)
    if (p.hasCallsign()) withCs++;
  CHECK(withCs >= 6);
  CHECK(pk.size() < kTxSeqLimit);  // 30 s stanno nel limite
}

static void testRxClean() {
  CHECK(receive(makeTx(75), {}) == expectAll(75));
  CHECK(receive(makeTx(750), {}) == expectAll(750));
  CHECK(receive(makeTx(12), {}) == expectAll(12));
  CHECK(receive(makeTx(0), {}).empty());
}

static void testRxRecovery() {
  RxStream rx;
  // Un pacchetto dati perso per gruppo viene recuperato dalla parita'.
  CHECK(receive(makeTx(75), {2}, &rx) == expectAll(75));
  CHECK(rx.recoveredPackets() == 1 && rx.lostFrames() == 0);
  CHECK(receive(makeTx(75), {2, 6}) == expectAll(75));
  // Il pacchetto END perso viene ricostruito (anche il flag END).
  CHECK(receive(makeTx(75), {8}, &rx) == expectAll(75));
  CHECK(rx.callsign()[0] != 0);
  // Perso il pacchetto START: nessun problema.
  CHECK(receive(makeTx(75), {0}) == expectAll(75));
  // Persa la sola parita': nessun effetto.
  CHECK(receive(makeTx(75), {4, 9}) == expectAll(75));
  // Gruppo finale parziale, pacchetto dati perso.
  CHECK(receive(makeTx(50), {5}) == expectAll(50));
}

static void testRxUnrecoverable() {
  RxStream rx;
  // Due pacchetti dello stesso gruppo persi: 24 frame mascherati, il resto resta allineato.
  auto got = receive(makeTx(75), {2, 3}, &rx);
  CHECK(got.size() == 75);
  for (int i = 0; i < 75; i++) {
    const bool lost = (i >= 12 && i < 36);
    CHECK(got[i] == (lost ? -1 : i + 1));
  }
  CHECK(rx.lostFrames() == 24);
}

static void testLateJoin() {
  // Ci si sintonizza a meta': i primi pacchetti non arrivano, la trasmissione
  // e' riprodotta dal primo pacchetto ricevuto.
  const auto pk = makeTx(100);
  std::set<int> drop;
  for (int i = 0; i < 7; i++) drop.insert(i);
  RxStream rx;
  const auto got = receive(pk, drop, &rx);
  CHECK(!got.empty());
  CHECK(got.back() == 100);
  // Frame contigui e crescenti, senza buchi.
  for (size_t i = 1; i < got.size(); i++) CHECK(got[i] == got[i - 1] + 1);
}

static void testSessionHandling() {
  const auto a = makeTx(24);
  RxStream rx;
  CHECK(rx.onPacket(a[0], 0));
  // Altra trasmissione mentre la prima e' in corso: ignorata.
  Packet other = a[1];
  other.session = 0x9999;
  CHECK(!rx.onPacket(other, 100));
  // Dopo il silenzio la nuova trasmissione sostituisce la vecchia.
  CHECK(rx.onPacket(other, 100 + kSessionTimeoutMs + 1));
  CHECK(rx.session() == 0x9999);
}

static void testEndLostTimesOut() {
  // Pacchetto END e relativa parita' persi: la ricezione si chiude per timeout.
  const auto pk = makeTx(75);
  RxStream rx;
  uint32_t now = 0;
  for (size_t i = 0; i + 2 < pk.size(); i++) {
    CHECK(rx.onPacket(pk[i], now));
  }
  bool ended = false;
  for (int i = 0; i < 400 && !ended; i++, now += kFrameMs) ended = rx.nextFrame(now).kind == RxStream::Kind::End;
  CHECK(ended);
  CHECK(rx.finished());
}

int main() {
  testTxLayout();
  testTxFrameBoundaryAndNoFec();
  testCallsignRepeat();
  testRxClean();
  testRxRecovery();
  testRxUnrecoverable();
  testLateJoin();
  testSessionHandling();
  testEndLostTimesOut();
  TEST_MAIN_END();
}
