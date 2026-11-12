// Collegamento fra nodi VoLoRa-IP: firma, sfida, anti-ripetizione, riavvio, manutenzione.
#include <cstring>
#include <vector>

#include "node_link.h"
#include "testing.h"

using namespace lcr;
typedef NodeLink::Kind K;

static uint32_t g_rng = 1000;
static uint32_t rngFn() { return g_rng += 7919; }

static NodeLink::Config cfg(uint32_t id, uint32_t peer, uint8_t k) {
  NodeLink::Config c;
  c.nodeId = id;
  c.peerId = peer;
  memset(c.key, k, sizeof(c.key));
  return c;
}

struct Pair {
  NodeLink a, b;
  uint32_t t = 0;
  Pair(uint32_t bootA = 0x1111, uint32_t bootB = 0x2222) : a(cfg(1, 2, 0x5A), bootA, rngFn), b(cfg(2, 1, 0x5A), bootB, rngFn) {}
};

typedef std::vector<uint8_t> Bytes;
static Bytes dg(const uint8_t* d, size_t n) { return Bytes(d, d + n); }

// Consegna un datagramma a un nodo e rimanda indietro le risposte finche' non ce ne sono piu'.
static void exchange(NodeLink& from, NodeLink& to, const Bytes& first, uint32_t& t, int depth = 0) {
  NodeLink::Rx& rx = to.onDatagram(first.data(), first.size(), t);
  std::vector<Bytes> replies;
  for (size_t i = 0; i < rx.nReplies; i++) replies.push_back(dg(rx.replies[i].data, rx.replies[i].len));
  if (depth > 6) return;
  for (const Bytes& r : replies) exchange(to, from, r, t, depth + 1);
}

static void handshake(Pair& p) {
  uint8_t buf[kNodeMaxDatagram];
  size_t n = p.a.makeHello(buf, sizeof(buf));
  exchange(p.a, p.b, dg(buf, n), p.t);
  // i due si sono autenticati; il collegamento e' attivo quando ognuno ha saputo dall'altro di essere stato accettato: bastano gli HELLO/KEEPALIVE
  n = p.b.makeKeepalive(buf, sizeof(buf));
  exchange(p.b, p.a, dg(buf, n), p.t);
  n = p.a.makeKeepalive(buf, sizeof(buf));
  exchange(p.a, p.b, dg(buf, n), p.t);
}

static void testHandshake() {
  Pair p;
  CHECK(!p.a.linkUp(0) && !p.b.linkUp(0));
  handshake(p);
  CHECK(p.a.peerAuthenticated() && p.b.peerAuthenticated() && p.a.linkUp(p.t) && p.b.linkUp(p.t));
  // basta un solo HELLO per far salire il collegamento da tutte e due le parti (sfida, risposta e stato si scambiano da soli)
  Pair q;
  uint8_t buf[kNodeMaxDatagram];
  const size_t n = q.a.makeHello(buf, sizeof(buf));
  exchange(q.a, q.b, dg(buf, n), q.t);
  CHECK(q.a.linkUp(q.t) && q.b.linkUp(q.t));
  CHECK(p.a.peerId() == 2 && p.b.peerId() == 1);
  CHECK(p.a.stats().authenticated == 1 && p.b.stats().authenticated == 1);
}

static void testPacketFlow() {
  Pair p;
  handshake(p);
  const uint8_t wire[10] = {0x21, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  uint8_t buf[kNodeMaxDatagram];
  size_t n = p.a.makePacket(wire, sizeof(wire), buf, sizeof(buf));
  CHECK(n == kNodeHeaderBytes + 10 + kNodeSigBytes);
  p.t += 100;
  NodeLink::Rx& rx = p.b.onDatagram(buf, n, p.t);
  CHECK(rx.kind == K::Packet && rx.packetLen == 10 && memcmp(rx.packet, wire, 10) == 0 && rx.nReplies == 0);
  // ripetuto (stessa registrazione): contatore gia' visto
  CHECK(p.b.onDatagram(buf, n, p.t + 1).kind == K::Replay && p.b.stats().replays == 1);
  // nell'altro verso
  n = p.b.makePacket(wire, sizeof(wire), buf, sizeof(buf));
  CHECK(p.a.onDatagram(buf, n, p.t + 2).kind == K::Packet);
  // fuori ordine ma piu' nuovo: accettato; piu' vecchio: no
  uint8_t b1[kNodeMaxDatagram], b2[kNodeMaxDatagram];
  const size_t n1 = p.a.makePacket(wire, 3, b1, sizeof(b1));
  const size_t n2 = p.a.makePacket(wire, 4, b2, sizeof(b2));
  CHECK(p.b.onDatagram(b2, n2, p.t + 3).kind == K::Packet);
  CHECK(p.b.onDatagram(b1, n1, p.t + 4).kind == K::Replay);
  // troppo lungo
  uint8_t big[300] = {0};
  CHECK(p.a.makePacket(big, 256, buf, sizeof(buf)) == 0 && p.a.makePacket(wire, 10, buf, 10) == 0);
}

static void testRejects() {
  Pair p;
  handshake(p);
  const uint8_t wire[6] = {1, 2, 3, 4, 5, 6};
  uint8_t buf[kNodeMaxDatagram];
  size_t n = p.a.makePacket(wire, sizeof(wire), buf, sizeof(buf));
  Bytes good = dg(buf, n);
  // firma sbagliata (un bit della firma, un bit del contenuto, un bit dell'intestazione)
  for (size_t pos : {n - 1, kNodeHeaderBytes + 2, size_t(13)}) {
    Bytes bad = good;
    bad[pos] ^= 1;
    CHECK(p.b.onDatagram(bad.data(), bad.size(), p.t).kind == K::BadSignature);
  }
  CHECK(p.b.stats().badSignature == 3);
  // chiave di un altro
  NodeLink other(cfg(1, 2, 0x77), 0x1111, rngFn);
  uint8_t ob[kNodeMaxDatagram];
  const size_t on = other.makePacket(wire, sizeof(wire), ob, sizeof(ob));
  CHECK(p.b.onDatagram(ob, on, p.t).kind == K::BadSignature);
  // troncato, magic sbagliato, lunghezza che non torna
  CHECK(p.b.onDatagram(good.data(), good.size() - 1, p.t).kind == K::Ignored);
  Bytes m = good;
  m[0] = 'X';
  CHECK(p.b.onDatagram(m.data(), m.size(), p.t).kind == K::Ignored);
  CHECK(p.b.onDatagram(good.data(), 5, p.t).kind == K::Ignored);
  Bytes ver = good;
  ver[2] = 9;
  CHECK(p.b.onDatagram(ver.data(), ver.size(), p.t).kind == K::Ignored);
  // un nodo che non e' quello atteso (id 3), un altro canale
  NodeLink c3(cfg(3, 2, 0x5A), 0x3333, rngFn);
  n = c3.makeHello(buf, sizeof(buf));
  CHECK(p.b.onDatagram(buf, n, p.t).kind == K::Ignored);
  NodeLink::Config cc = cfg(1, 2, 0x5A);
  cc.channel = 4;
  NodeLink ch(cc, 0x1111, rngFn);
  n = ch.makePacket(wire, sizeof(wire), buf, sizeof(buf));
  CHECK(p.b.onDatagram(buf, n, p.t).kind == K::Ignored);
  // i propri datagrammi (stesso id) non si accettano
  NodeLink self(cfg(2, 1, 0x5A), 0x2222, rngFn);
  n = self.makeHello(buf, sizeof(buf));
  CHECK(p.b.onDatagram(buf, n, p.t).kind == K::Ignored);
}

static void testRebootAndReplay() {
  Pair p;
  handshake(p);
  uint8_t buf[kNodeMaxDatagram];
  const uint8_t wire[6] = {1, 2, 3, 4, 5, 6};
  size_t n = p.a.makePacket(wire, sizeof(wire), buf, sizeof(buf));
  const Bytes old = dg(buf, n);  // registrazione di un pacchetto dell'avvio 0x1111
  CHECK(p.b.onDatagram(old.data(), old.size(), p.t).kind == K::Packet);
  // A si riavvia: nuovo id di avvio, contatore da capo
  NodeLink a2(cfg(1, 2, 0x5A), 0xAAAA, rngFn);
  p.t += 5000;
  n = a2.makePacket(wire, sizeof(wire), buf, sizeof(buf));  // contatore 1 con un avvio nuovo
  NodeLink::Rx& rx = p.b.onDatagram(buf, n, p.t);
  CHECK(rx.kind == K::Unknown && rx.nReplies == 1);  // non accettato: sfida
  Bytes chal = dg(rx.replies[0].data, rx.replies[0].len);
  // la vecchia registrazione dell'avvio precedente, ora: stesso id di avvio ma contatore gia' visto, oppure sfida (se l'avvio e' cambiato)
  CHECK(p.b.onDatagram(old.data(), old.size(), p.t + 10).kind == K::Replay);  // B non ha ancora accettato il nuovo avvio
  // A risponde alla sfida e il collegamento riparte con il nuovo avvio
  exchange(p.b, a2, chal, p.t);
  CHECK(p.b.peerAuthenticated() && a2.peerAuthenticated());
  p.t += 100;
  n = a2.makePacket(wire, sizeof(wire), buf, sizeof(buf));
  CHECK(p.b.onDatagram(buf, n, p.t).kind == K::Packet);
  // adesso la vecchia registrazione (avvio 0x1111) non entra piu': avvio sconosciuto, nessun pacchetto
  NodeLink::Rx& r2 = p.b.onDatagram(old.data(), old.size(), p.t + 5000);
  CHECK(r2.kind == K::Unknown);
}

static void testAttackerWithoutKey() {
  Pair p;
  uint8_t buf[kNodeMaxDatagram];
  // handshake a mano, registrando i datagrammi che passano
  size_t n = p.a.makeHello(buf, sizeof(buf));
  NodeLink::Rx* rx = &p.b.onDatagram(buf, n, 0);
  Bytes chal1 = dg(rx->replies[0].data, rx->replies[0].len);
  rx = &p.a.onDatagram(chal1.data(), chal1.size(), 1);
  CHECK(rx->nReplies == 2);
  const Bytes resp1 = dg(rx->replies[0].data, rx->replies[0].len);  // la risposta di A alla sfida di B: un attaccante la puo' registrare
  CHECK(p.b.onDatagram(resp1.data(), resp1.size(), 2).kind == K::Control && p.b.peerAuthenticated());
  const uint32_t authBefore = p.b.stats().authenticated;
  // A si riavvia; B lancia una sfida NUOVA (dopo almeno un secondo): la risposta registrata prima non vale
  NodeLink a2(cfg(1, 2, 0x5A), 0xDDDD, rngFn);
  n = a2.makeHello(buf, sizeof(buf));
  rx = &p.b.onDatagram(buf, n, 5000);
  CHECK(rx->kind == K::Unknown && rx->nReplies == 1);
  const Bytes chal2 = dg(rx->replies[0].data, rx->replies[0].len);
  CHECK(chal2 != chal1);  // un numero nuovo
  CHECK(p.b.onDatagram(resp1.data(), resp1.size(), 5001).kind != K::Control);  // la risposta vecchia, riascoltata: non autentica niente
  CHECK(p.b.stats().authenticated == authBefore);
  // chi non ha la chiave non puo' nemmeno farsi sfidare: firma sbagliata, nessuna sfida emessa
  NodeLink evil(cfg(1, 2, 0x11), 0xDDDD, rngFn);
  n = evil.makeHello(buf, sizeof(buf));
  const uint32_t challengesBefore = p.b.stats().challenges;
  CHECK(p.b.onDatagram(buf, n, 9000).kind == K::BadSignature && p.b.stats().challenges == challengesBefore);
  // la risposta giusta di a2 alla sfida giusta autentica il nuovo avvio
  rx = &a2.onDatagram(chal2.data(), chal2.size(), 5002);
  CHECK(rx->nReplies >= 1);
  const Bytes resp2 = dg(rx->replies[0].data, rx->replies[0].len);
  CHECK(p.b.onDatagram(resp2.data(), resp2.size(), 5003).kind == K::Control && p.b.stats().authenticated == authBefore + 1);
}

static void testChallengeRateLimit() {
  Pair p;
  uint8_t buf[kNodeMaxDatagram];
  NodeLink a2(cfg(1, 2, 0x5A), 0xCCCC, rngFn);
  size_t n = a2.makeHello(buf, sizeof(buf));
  CHECK(p.b.onDatagram(buf, n, 0).nReplies == 1);
  n = a2.makeHello(buf, sizeof(buf));
  CHECK(p.b.onDatagram(buf, n, 500).nReplies == 0 && p.b.stats().unknown == 2);  // troppo presto
  n = a2.makeHello(buf, sizeof(buf));
  CHECK(p.b.onDatagram(buf, n, kNodeChallengeGapMs).nReplies == 1);
  CHECK(p.b.stats().challenges == 2);
  // la risposta a una sfida vecchia (scaduta) non vale
  Pair q;
  n = q.a.makeHello(buf, sizeof(buf));
  NodeLink::Rx& rx = q.b.onDatagram(buf, n, 0);
  Bytes chal = dg(rx.replies[0].data, rx.replies[0].len);
  NodeLink::Rx& rr = q.a.onDatagram(chal.data(), chal.size(), 1);
  CHECK(rr.nReplies >= 1);
  Bytes resp = dg(rr.replies[0].data, rr.replies[0].len);
  CHECK(q.b.onDatagram(resp.data(), resp.size(), kNodeChallengeLifeMs + 5).kind == K::Ignored && !q.b.peerAuthenticated());
  CHECK(q.b.onDatagram(resp.data(), resp.size(), 5).kind == K::Control && q.b.peerAuthenticated());  // in tempo (ora b e' "indietro", prova solo la regola)
}

static void testMaintenance() {
  Pair p;
  uint8_t buf[kNodeMaxDatagram];
  CHECK(p.a.maintenance(0, buf, sizeof(buf)) > 0);          // il primo HELLO subito
  CHECK(p.a.maintenance(1000, buf, sizeof(buf)) == 0);
  CHECK(p.a.maintenance(2000, buf, sizeof(buf)) > 0);       // poi ogni 2 s
  CHECK(buf[3] == kNodeHello);
  // collegamento attivo: keepalive ogni 20 s
  Pair q;
  handshake(q);
  uint32_t t = q.t;
  size_t n = q.a.maintenance(t, buf, sizeof(buf));
  CHECK(n > 0 && buf[3] == kNodeKeepalive);
  CHECK(q.a.maintenance(t + 10000, buf, sizeof(buf)) == 0);
  CHECK(q.a.maintenance(t + 20000, buf, sizeof(buf)) > 0 && buf[3] == kNodeKeepalive);
  // il keepalive e' accettato dal nodo autenticato
  n = q.a.makeKeepalive(buf, sizeof(buf));
  CHECK(q.b.onDatagram(buf, n, t + 20001).kind == K::Control);
  // senza datagrammi per 60 s il collegamento e' giu' e si torna agli HELLO
  CHECK(q.a.linkUp(t + 20000) && !q.a.linkUp(t + kNodeDeadMs + 10));
  CHECK(q.a.maintenance(t + kNodeDeadMs + 10000, buf, sizeof(buf)) > 0 && buf[3] == kNodeHello);
}

int main() {
  testHandshake();
  testPacketFlow();
  testRejects();
  testRebootAndReplay();
  testAttackerWithoutKey();
  testChallengeRateLimit();
  testMaintenance();
  TEST_MAIN_END();
}
