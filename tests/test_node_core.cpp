// Il nodo su Wi-Fi: due nodi che si portano i pacchetti radio a vicenda, con rete finta (perdite, duplicati, collegamento che cade).
#include <cmath>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>

#include "fakes.h"
#include "node_core.h"
#include "packet.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;

static uint32_t g_rng = 4242;
static uint32_t rngFn() { return g_rng += 104729; }

typedef std::vector<uint8_t> Bytes;

// Rete finta: due code di datagrammi (da A a B e da B ad A); si possono perdere o duplicare.
struct Net {
  std::deque<Bytes> toB, toA;
  bool up = true;     // se false si perde tutto
  int dropEvery = 0;  // perde un datagramma ogni N (solo PACKET)
  int count = 0;
  bool duplicate = false;
};

struct Side : public INodeNet {
  fake::Air air;
  fake::Radio radio{air, 0};
  std::unique_ptr<NodeCore> core;
  std::deque<Bytes>* out = nullptr;
  Net* net = nullptr;
  bool isA = true;
  void sendDatagram(const uint8_t* d, size_t n) override {
    if (!net->up) return;
    if (n > 3 && d[3] == kNodePacket && net->dropEvery && (++net->count % net->dropEvery) == 0) return;
    out->push_back(Bytes(d, d + n));
    if (net->duplicate && n > 3 && d[3] == kNodePacket) out->push_back(Bytes(d, d + n));
  }
};

struct Rig {
  Net net;
  Side a, b;
  uint32_t t = 0;
  bool txA = true, txB = true;
  explicit Rig(uint8_t keyB = 0x42) {
    NodeLink::Config ca, cb;
    ca.nodeId = 1;
    ca.peerId = 2;
    cb.nodeId = 2;
    cb.peerId = 1;
    memset(ca.key, 0x42, sizeof(ca.key));
    memset(cb.key, keyB, sizeof(cb.key));
    a.net = b.net = &net;
    a.out = &net.toB;
    b.out = &net.toA;
    a.isA = true;
    b.isA = false;
    a.core.reset(new NodeCore(a.radio, a, ca, 0xA0A0, rngFn, kModeCodec2_1200));
    b.core.reset(new NodeCore(b.radio, b, cb, 0xB0B0, rngFn, kModeCodec2_1200));
  }
  void step() {
    while (!net.toA.empty()) {
      const Bytes d = net.toA.front();
      net.toA.pop_front();
      a.core->onDatagram(d.data(), d.size(), t);
    }
    while (!net.toB.empty()) {
      const Bytes d = net.toB.front();
      net.toB.pop_front();
      b.core->onDatagram(d.data(), d.size(), t);
    }
    a.core->tick(t, txA);
    b.core->tick(t, txB);
  }
  void run(uint32_t ms, uint32_t stepMs = 10) {
    const uint32_t end = t + ms;
    while (t < end) {
      step();
      t += stepMs;
    }
  }
};

static std::vector<Packet> makeMessage(uint16_t session, int frames, const char* call = "IU5HJU") {
  std::vector<Packet> v;
  TxSession tx(call, session, true);
  tx.setExpectedTotal(TxSession::expectedPackets(frames, true));
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

static Bytes wireOf(const Packet& p) {
  uint8_t b[kMaxWireBytes];
  const size_t n = encodePacket(p, b, sizeof(b));
  return Bytes(b, b + n);
}

static void injectToA(Rig& r, const std::vector<Packet>& msg, uint32_t gapMs = 100) {
  for (const Packet& p : msg) {
    r.a.air.inbox[0].push_back(wireOf(p));
    r.run(gapMs);
  }
}

static void testLinkComesUp() {
  Rig r;
  CHECK(!r.a.core->linkUp(0));
  r.run(3000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  CHECK(r.a.core->link().stats().authenticated == 1 && r.b.core->link().stats().authenticated == 1);
}

static void testVoiceAcrossTheNet() {
  Rig r;
  r.run(3000);
  const auto msg = makeMessage(0x1111, 60);
  injectToA(r, msg);
  r.run(2000);
  // B ha trasmesso in radio lo stesso messaggio, con il flag RELAYED e la stessa sessione
  CHECK(r.b.air.sent[0] == static_cast<int>(msg.size()));
  CHECK(r.a.core->stats().toNet == msg.size() && r.b.core->stats().fromNet == msg.size() && r.b.core->stats().transmitted == msg.size());
  for (size_t i = 0; i < msg.size(); i++) {
    Packet p;
    CHECK(decodePacket(r.b.air.log[0][i].data(), r.b.air.log[0][i].size(), p));
    CHECK((p.flags & kFlagRelayed) && p.session == msg[i].session && p.seq == msg[i].seq && p.payloadLen == msg[i].payloadLen);
    CHECK(memcmp(p.payload, msg[i].payload, p.payloadLen) == 0);
  }
  CHECK(r.a.air.sent[0] == 0);  // il nodo A non ha trasmesso niente (la voce era sua)
}

static void testOtherDirectionAndText() {
  Rig r;
  r.run(3000);
  Packet text;
  text.mode = kModeText;
  text.session = 0x7777;
  text.flags = kFlagStart | kFlagEnd;
  strcpy(text.callsign, "IU5HJU");
  strcpy(reinterpret_cast<char*>(text.payload), "Arrivo tra 10 minuti");
  text.payloadLen = 20;
  r.b.air.inbox[0].push_back(wireOf(text));
  r.run(1000);
  CHECK(r.a.air.sent[0] == 1 && r.a.core->stats().transmitted == 1);
  Packet p;
  CHECK(decodePacket(r.a.air.log[0][0].data(), r.a.air.log[0][0].size(), p) && p.mode == kModeText && (p.flags & kFlagRelayed) && strcmp(p.callsign, "IU5HJU") == 0);
}

static void testNoLoops() {
  Rig r;
  r.run(3000);
  auto msg = makeMessage(0x2222, 30);
  for (Packet& p : msg) p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);  // gia' ripetuto da un altro ripetitore
  injectToA(r, msg);
  r.run(2000);
  CHECK(r.a.core->stats().toNet == 0 && r.b.air.sent[0] == 0);
  // altri modi: la prova di collegamento, i messaggi (SF diverso), gli annunci
  Packet t;
  t.mode = kModeTest;
  t.session = 0x3333;
  t.flags = kFlagStart | kFlagEnd | kFlagCallsign;
  r.a.air.inbox[0].push_back(wireOf(t));
  r.run(500);
  CHECK(r.a.core->stats().toNet == 0);
  // dalla rete: un pacchetto del modo Test non si trasmette
  CHECK(r.b.core->stats().transmitted == 0);
  // una stessa sessione e seq due volte dalla radio: una sola ripetizione
  Rig q;
  q.run(3000);
  const auto m2 = makeMessage(0x4444, 12);
  q.a.air.inbox[0].push_back(wireOf(m2[0]));
  q.a.air.inbox[0].push_back(wireOf(m2[0]));
  q.run(1000);
  CHECK(q.a.core->stats().toNet == 1);
}

static void testLinkDownAndRecovery() {
  Rig r;
  r.run(3000);
  r.net.up = false;
  const auto msg = makeMessage(0x5555, 20);
  injectToA(r, msg);
  r.run(1000);
  CHECK(r.b.air.sent[0] == 0);  // la rete e' caduta: A crede ancora attivo il collegamento (60 s) e manda nel vuoto, B non riceve niente
  // dopo 60 s senza datagrammi il collegamento e' giu' e ogni pacchetto da ripetere si conta come perso
  r.run(kNodeDeadMs + 2000);
  CHECK(!r.a.core->linkUp(r.t));
  const uint32_t before = r.a.core->stats().droppedLinkDown;
  injectToA(r, makeMessage(0x5656, 12));
  CHECK(r.a.core->stats().droppedLinkDown > before);
  // la rete torna: il collegamento si riforma da solo (HELLO, sfida, risposta)
  r.net.up = true;
  r.run(8000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  const auto m3 = makeMessage(0x5757, 12);
  const int sent = r.b.air.sent[0];
  injectToA(r, m3);
  r.run(2000);
  CHECK(r.b.air.sent[0] == sent + static_cast<int>(m3.size()));
}

static void testRebootOfOneNode() {
  Rig r;
  r.run(3000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  // B si riavvia: nuovo id di avvio, contatori da capo, e la rete ha ancora in giro roba vecchia
  NodeLink::Config cb;
  cb.nodeId = 2;
  cb.peerId = 1;
  memset(cb.key, 0x42, sizeof(cb.key));
  r.b.core.reset(new NodeCore(r.b.radio, r.b, cb, 0xB1B1, rngFn, kModeCodec2_1200));
  r.run(8000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  const auto msg = makeMessage(0xABAB, 24);
  injectToA(r, msg);
  r.run(2000);
  CHECK(r.b.air.sent[0] == static_cast<int>(msg.size()));
  // e nell'altro verso
  r.b.air.inbox[0].push_back(wireOf(makeMessage(0xBCBC, 12)[0]));
  r.run(1000);
  CHECK(r.a.air.sent[0] == 1);
}

static void testWrongKey() {
  Rig r(0x99);  // il nodo B ha un'altra chiave
  r.run(10000);
  CHECK(!r.a.core->linkUp(r.t) && !r.b.core->linkUp(r.t));
  CHECK(r.a.core->link().stats().badSignature > 0);
  injectToA(r, makeMessage(0x6666, 12));
  CHECK(r.b.air.sent[0] == 0);
}

static void testNetworkLossAndDuplicates() {
  Rig r;
  r.run(3000);
  r.net.dropEvery = 4;  // si perde un pacchetto su quattro
  const auto msg = makeMessage(0x7777, 48);
  injectToA(r, msg);
  r.run(2000);
  CHECK(r.b.core->stats().fromNet >= msg.size() * 2 / 3 && r.b.core->stats().fromNet < msg.size());
  // duplicati in rete (UDP): il contatore li scarta
  Rig q;
  q.run(3000);
  q.net.duplicate = true;
  const auto m2 = makeMessage(0x7878, 24);
  injectToA(q, m2);
  q.run(2000);
  CHECK(q.b.core->stats().fromNet == m2.size() && q.b.core->link().stats().replays >= m2.size());
}

static void testBusyChannelAndNoCallsign() {
  Rig r;
  r.run(3000);
  r.b.air.busy = true;  // una stazione locale occupa il canale del nodo B
  const auto msg = makeMessage(0x8888, 20);
  injectToA(r, msg);
  r.run(1000);
  CHECK(r.b.air.sent[0] == 0 && r.b.core->stats().busyDeferred > 0 && r.b.core->queue().size() > 0);
  r.b.air.busy = false;
  r.run(3000);
  CHECK(r.b.air.sent[0] == static_cast<int>(msg.size()));  // poi parte tutto, nello stesso ordine
  // N0CALL su B: niente in radio, tutto scartato
  Rig q;
  q.txB = false;
  q.run(3000);
  injectToA(q, makeMessage(0x8989, 12));
  q.run(2000);
  CHECK(q.b.air.sent[0] == 0 && q.b.core->stats().discardedNoTx > 0);
}

static void testQueueFull() {
  Rig r;
  r.run(3000);
  r.b.air.busy = true;
  injectToA(r, makeMessage(0x9999, 400), 20);
  r.run(500);
  CHECK(r.b.core->queue().size() == TxQueue::kSlots && r.b.core->queue().dropped() > 0);
}

// Il nodo con il profilo Standard (Codec2 3200, modo 4): porta quel modo e il testo, non il Codec2 1200.
static void testStandardMode() {
  Rig r;
  NodeLink::Config ca, cb;
  ca.nodeId = 1;
  ca.peerId = 2;
  cb.nodeId = 2;
  cb.peerId = 1;
  memset(ca.key, 0x42, sizeof(ca.key));
  memset(cb.key, 0x42, sizeof(cb.key));
  r.a.core.reset(new NodeCore(r.a.radio, r.a, ca, 0x1A1A, rngFn, kModeCodec2_3200));
  r.b.core.reset(new NodeCore(r.b.radio, r.b, cb, 0x1B1B, rngFn, kModeCodec2_3200));
  r.run(4000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  // un messaggio a 3200 (16 byte per frame) passa
  std::vector<Packet> v;
  TxSession tx("IU5HJU", 0x3200, true, kModeCodec2_3200);
  tx.setExpectedTotal(TxSession::expectedPackets(30, true));
  auto drain = [&]() {
    Packet p;
    while (tx.popPacket(p)) v.push_back(p);
  };
  tx.begin();
  drain();
  for (int i = 0; i < 30; i++) {
    uint8_t f[kMaxFrameBytes];
    for (size_t k = 0; k < kMaxFrameBytes; k++) f[k] = static_cast<uint8_t>(i * 5 + k + 1);
    tx.pushFrame(f);
    drain();
  }
  tx.finish();
  drain();
  CHECK(v.size() > 4);
  injectToA(r, v);
  r.run(2000);
  CHECK(r.b.air.sent[0] == static_cast<int>(v.size()));
  // il Codec2 1200 non e' il modo del nodo: non si ripete
  const auto old = makeMessage(0x1200, 12);
  const int sent = r.b.air.sent[0];
  injectToA(r, old);
  r.run(1000);
  CHECK(r.b.air.sent[0] == sent && r.a.core->stats().toNet == v.size());
}

static void testBadDatagrams() {
  Rig r;
  r.run(3000);
  // un datagramma ben firmato che porta spazzatura al posto di un pacchetto radio
  NodeLink::Config c;
  c.nodeId = 1;
  c.peerId = 2;
  memset(c.key, 0x42, sizeof(c.key));
  // si riusa il collegamento di A: costruisce un PACKET con contenuto non valido
  uint8_t junk[10] = {0xFF, 1, 2, 3};
  uint8_t buf[kNodeMaxDatagram];
  const size_t n = const_cast<NodeLink&>(r.a.core->link()).makePacket(junk, sizeof(junk), buf, sizeof(buf));
  r.b.core->onDatagram(buf, n, r.t);
  CHECK(r.b.core->stats().netBad == 1 && r.b.core->queue().size() == 0);
}

static Packet decodeSent(const Bytes& w) {
  Packet p;
  CHECK(decodePacket(w.data(), w.size(), p));
  return p;
}

static void testIdentification() {
  // Identificativo periodico: il primo dopo 15 s, poi a ogni intervallo, con il nominativo e il testo "Nodo VoLoRa"
  Rig r;
  r.b.core->setIdentification("IU5HJU", 60000, 433900);
  r.run(14000);
  CHECK(r.b.air.sent[0] == 0 && r.b.core->stats().beacons == 0);
  r.run(2000);
  CHECK(r.b.air.sent[0] == 1 && r.b.core->stats().beacons == 1);
  const Packet id = decodeSent(r.b.air.log[0][0]);
  CHECK(id.mode == kModeText && id.seq == kTextSeqBeacon && (id.flags & kFlagStart) && (id.flags & kFlagEnd));
  CHECK(!(id.flags & kFlagRelayed) && strcmp(id.callsign, "IU5HJU") == 0);
  CHECK(id.payloadLen == 11 && memcmp(id.payload, "Nodo VoLoRa", 11) == 0);
  CHECK(r.a.air.sent[0] == 0);  // l'altro nodo non e' configurato: niente identificativo
  r.run(58000);
  CHECK(r.b.air.sent[0] == 1);  // non prima dell'intervallo
  r.run(4000);
  CHECK(r.b.air.sent[0] == 2 && r.b.core->stats().beacons == 2);
  // sessioni diverse
  CHECK(decodeSent(r.b.air.log[0][1]).session != id.session);
  // l'identificativo non passa in rete
  CHECK(r.b.core->stats().toNet == 0 && r.a.core->stats().fromNet == 0);

  // spento (intervallo 0) o con N0CALL: niente
  Rig q;
  q.b.core->setIdentification("IU5HJU", 0, 433900);
  q.run(40000);
  CHECK(q.b.air.sent[0] == 0);
  Rig n;
  n.txB = false;
  n.b.core->setIdentification("IU5HJU", 60000, 433900);
  n.run(40000);
  CHECK(n.b.air.sent[0] == 0 && n.b.core->stats().beacons == 0);
}

static void testIdentificationWaitsForQuiet() {
  // Canale occupato: si rimanda, e parte appena e' libero (e senza attivita' radio negli ultimi 3 s)
  Rig r;
  r.b.core->setIdentification("IU5HJU", 60000, 433900);
  r.b.air.busy = true;
  r.run(20000);
  CHECK(r.b.air.sent[0] == 0 && r.b.core->stats().beaconDeferred == 1);
  r.b.air.busy = false;
  r.run(200);
  CHECK(r.b.air.sent[0] == 1);

  // Una stazione locale parla (pacchetti ricevuti in radio): l'identificativo aspetta 3 s dopo l'ultimo
  Rig q;
  q.b.core->setIdentification("IU5HJU", 60000, 433900);
  q.run(14000);
  const auto msg = makeMessage(0x2222, 60);
  for (int i = 0; i < 40; i++) {  // un pacchetto ogni 250 ms per 10 s, a cavallo dell'ora dell'identificativo
    q.b.air.inbox[0].push_back(wireOf(msg[i % msg.size()]));
    q.run(250);
  }
  CHECK(q.b.air.sent[0] == 0);
  q.run(2000);
  CHECK(q.b.air.sent[0] == 0);
  q.run(2000);
  CHECK(q.b.air.sent[0] == 1);
}

static void testMorseId() {
  Rig r;
  r.b.core->setIdentification("IU5HJU", 60000, 433900);
  r.b.core->setMorseId(true, 15, 700.0f, 2.5f);
  r.run(16000);
  CHECK(r.b.air.sent[0] == 1 && r.b.radio.fskBegins == 1 && r.b.core->morseActive() && r.b.radio.inFsk);
  CHECK(std::fabs(r.b.radio.fskMhz - 433.9f) < 1e-3 && std::fabs(r.b.radio.fskDev - 2.5f) < 1e-4);
  r.run(20000);
  CHECK(!r.b.core->morseActive() && !r.b.radio.inFsk && r.b.radio.fskEnds == 1 && r.b.core->stats().morseIds == 1);
  CHECK(r.b.radio.fskBursts.size() > 10 && r.b.radio.fskBursts[0].pattern == 0xFF && r.b.radio.fskBursts.back().pattern == 0xFF);
  // al periodo successivo ancora, una volta sola
  r.run(60000);
  CHECK(r.b.radio.fskBegins == 2 && r.b.radio.fskEnds == 2 && r.b.air.sent[0] == 2);

  // un pacchetto dalla rete interrompe il Morse (poi il pacchetto parte)
  Rig q;
  q.b.core->setIdentification("IU5HJU", 60000, 433900);
  q.b.core->setMorseId(true);
  q.run(16000);
  CHECK(q.b.core->morseActive());
  const auto msg = makeMessage(0x3333, 12);
  injectToA(q, msg);
  q.run(2000);
  CHECK(!q.b.core->morseActive() && !q.b.radio.inFsk && q.b.core->stats().morseAborted == 1 && q.b.core->stats().morseIds == 0);
  CHECK(q.b.air.sent[0] == 1 + static_cast<int>(msg.size()));

  // senza Morse acceso: solo il testo
  Rig p;
  p.b.core->setIdentification("IU5HJU", 60000, 433900);
  p.run(20000);
  CHECK(p.b.air.sent[0] == 1 && p.b.radio.fskBegins == 0);
}

int main() {
  testLinkComesUp();
  testVoiceAcrossTheNet();
  testOtherDirectionAndText();
  testNoLoops();
  testLinkDownAndRecovery();
  testRebootOfOneNode();
  testWrongKey();
  testNetworkLossAndDuplicates();
  testBusyChannelAndNoCallsign();
  testQueueFull();
  testStandardMode();
  testBadDatagrams();
  testIdentification();
  testIdentificationWaitsForQuiet();
  testMorseId();
  TEST_MAIN_END();
}
