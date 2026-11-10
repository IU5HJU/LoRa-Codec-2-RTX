// Collegamento seriale fra le schede del ripetitore e coda di trasmissione.
#include <cstring>
#include <vector>

#include "packet.h"
#include "repeater_link.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;

static void testCrc() {
  const uint8_t msg[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(linkCrc16(msg, sizeof(msg)) == 0x29B1);  // CRC-16/CCITT-FALSE: valore di riferimento
}

static void testRoundTrip() {
  uint8_t data[100];
  for (size_t i = 0; i < sizeof(data); i++) data[i] = static_cast<uint8_t>(i * 3 + 1);
  uint8_t buf[kLinkMaxFrame];
  const size_t n = encodeLinkFrame(kLinkPacket, data, sizeof(data), buf, sizeof(buf));
  CHECK(n == 5 + sizeof(data));
  LinkParser p;
  bool done = false;
  for (size_t i = 0; i < n; i++) done = p.feed(buf[i]);
  CHECK(done && p.frame().type == kLinkPacket && p.frame().len == sizeof(data) && memcmp(p.frame().data, data, sizeof(data)) == 0);
  // Troppi dati o buffer piccolo
  CHECK(encodeLinkFrame(kLinkPacket, data, kLinkMaxData + 1, buf, sizeof(buf)) == 0);
  CHECK(encodeLinkFrame(kLinkPacket, data, 10, buf, 14) == 0);
  // Frame vuoto (battito senza dati)
  const size_t m = encodeLinkFrame(kLinkHeartbeat, nullptr, 0, buf, sizeof(buf));
  CHECK(m == 5);
  LinkParser q;
  bool ok = false;
  for (size_t i = 0; i < m; i++) ok = q.feed(buf[i]);
  CHECK(ok && q.frame().type == kLinkHeartbeat && q.frame().len == 0);
}

static void testResync() {
  uint8_t a[kLinkMaxFrame], b[kLinkMaxFrame];
  const uint8_t d1[] = {1, 2, 3, kLinkStart, 4, 5};  // il dato contiene 0x7E
  const uint8_t d2[] = {9, 8, 7};
  const size_t na = encodeLinkFrame(kLinkPacket, d1, sizeof(d1), a, sizeof(a));
  const size_t nb = encodeLinkFrame(kLinkPacket, d2, sizeof(d2), b, sizeof(b));
  // rumore, un frame con un bit sbagliato (scartato), rumore con un falso inizio, poi due frame buoni
  std::vector<uint8_t> stream = {0x00, 0xFF, kLinkStart, 0x01, 0x02};
  std::vector<uint8_t> bad(a, a + na);
  bad[4] ^= 0x10;
  stream.insert(stream.end(), bad.begin(), bad.end());
  stream.push_back(0x33);
  stream.insert(stream.end(), a, a + na);
  stream.insert(stream.end(), b, b + nb);
  LinkParser p;
  std::vector<std::vector<uint8_t>> got;
  for (uint8_t byte : stream) {
    if (p.feed(byte)) got.emplace_back(p.frame().data, p.frame().data + p.frame().len);
  }
  CHECK(got.size() == 2);
  CHECK(got.size() == 2 && got[0] == std::vector<uint8_t>(d1, d1 + sizeof(d1)) && got[1] == std::vector<uint8_t>(d2, d2 + sizeof(d2)));
  CHECK(p.crcErrors() >= 1);
}

static void testQueue() {
  TxQueue q;
  uint8_t out[kLinkMaxData];
  size_t len = 0;
  uint8_t a[10], b[20];
  memset(a, 0xA1, sizeof(a));
  memset(b, 0xB2, sizeof(b));
  CHECK(!q.next(0, out, sizeof(out), len));  // vuota
  CHECK(q.push(a, sizeof(a)) && q.push(b, sizeof(b)));
  CHECK(q.next(1000, out, sizeof(out), len) && len == 10 && out[0] == 0xA1);  // in ordine
  q.sent(1000);
  CHECK(!q.next(1000 + kInterPacketGapMs - 1, out, sizeof(out), len));       // pausa minima
  CHECK(q.next(1000 + kInterPacketGapMs, out, sizeof(out), len) && len == 20 && out[0] == 0xB2);
  q.sent(1100);
  CHECK(q.size() == 0);
  // Coda piena: i pacchetti in piu' si perdono e si contano
  for (size_t i = 0; i < TxQueue::kSlots; i++) CHECK(q.push(a, sizeof(a)));
  CHECK(!q.push(a, sizeof(a)) && !q.push(b, sizeof(b)));
  CHECK(q.dropped() == 2 && q.size() == TxQueue::kSlots);
  CHECK(!q.push(a, 0) && !q.push(a, kLinkMaxData + 1));
}

// Catena dei byte: pacchetto radio vero -> frame seriale -> coda -> pacchetto ritrasmesso identico.
static void testPacketThroughLink() {
  TxSession tx("IU5HJU", 77, true);
  tx.begin();
  Packet p;
  CHECK(tx.popPacket(p));
  uint8_t wire[kMaxWireBytes];
  const size_t wn = encodePacket(p, wire, sizeof(wire));
  uint8_t frame[kLinkMaxFrame];
  const size_t fn = encodeLinkFrame(kLinkPacket, wire, wn, frame, sizeof(frame));
  LinkParser lp;
  TxQueue q;
  for (size_t i = 0; i < fn; i++) {
    if (lp.feed(frame[i]) && lp.frame().type == kLinkPacket) CHECK(q.push(lp.frame().data, lp.frame().len));
  }
  uint8_t out[kMaxWireBytes];
  size_t len = 0;
  CHECK(q.next(0, out, sizeof(out), len) && len == wn && memcmp(out, wire, wn) == 0);
}

int main() {
  testCrc();
  testRoundTrip();
  testResync();
  testQueue();
  testPacketThroughLink();
  TEST_MAIN_END();
}
