#include <cstring>

#include "packet.h"
#include "testing.h"

using namespace lcr;

static Packet makeData(uint8_t seq, size_t frames, uint8_t fill, uint8_t flags = 0) {
  Packet p;
  p.session = 0xBEEF;
  p.seq = seq;
  p.flags = flags;
  p.payloadLen = static_cast<uint8_t>(frames * kFrameBytes);
  for (size_t i = 0; i < p.payloadLen; i++) p.payload[i] = static_cast<uint8_t>(fill + i * 7);
  return p;
}

static bool samePayload(const Packet& a, const Packet& b) {
  return a.payloadLen == b.payloadLen && memcmp(a.payload, b.payload, a.payloadLen) == 0;
}

static void testRoundTrip() {
  Packet p = makeData(3, 12, 1, kFlagStart);
  strcpy(p.callsign, "IU5HJU");
  uint8_t buf[kMaxWireBytes];
  const size_t n = encodePacket(p, buf, sizeof(buf));
  CHECK(n == 5 + 1 + 6 + 72);
  Packet q;
  CHECK(decodePacket(buf, n, q));
  CHECK(q.session == 0xBEEF && q.seq == 3 && q.mode == kModeCodec2_1200);
  CHECK(strcmp(q.callsign, "IU5HJU") == 0);
  CHECK(q.flags == (kFlagStart | kFlagCallsign));
  CHECK(samePayload(p, q));

  Packet r = makeData(4, 0, 0);
  const size_t m = encodePacket(r, buf, sizeof(buf));
  CHECK(m == 5);
  CHECK(decodePacket(buf, m, q));
  CHECK(!q.hasCallsign() && q.payloadLen == 0);
}

static void testRejects() {
  uint8_t buf[kMaxWireBytes];
  Packet p = makeData(1, 2, 9);
  const size_t n = encodePacket(p, buf, sizeof(buf));
  Packet q;
  CHECK(decodePacket(buf, n, q));
  CHECK(!decodePacket(buf, 4, q));         // troppo corto
  CHECK(!decodePacket(buf, n - 1, q));     // payload non multiplo di 6
  uint8_t bad[kMaxWireBytes];
  memcpy(bad, buf, n);
  bad[0] = 0x21;                           // versione 2
  CHECK(!decodePacket(bad, n, q));
  memcpy(bad, buf, n);
  bad[4] = 0x80;                           // flag riservato
  CHECK(!decodePacket(bad, n, q));
  memcpy(bad, buf, n);
  bad[3] = kMaxPackets;                    // seq fuori range
  CHECK(!decodePacket(bad, n, q));

  Packet c = makeData(0, 0, 0);
  strcpy(c.callsign, "iu5hju");            // minuscolo non valido
  CHECK(encodePacket(c, buf, sizeof(buf)) == 0);
  memcpy(c.callsign, "TROPPOLUN", 9);  // 9 caratteri, senza terminatore valido
  CHECK(encodePacket(c, buf, sizeof(buf)) == 0);
  strcpy(c.callsign, "IU5HJU/P");
  CHECK(encodePacket(c, buf, sizeof(buf)) > 0);
  CHECK(encodePacket(c, buf, 6) == 0);     // buffer troppo piccolo

  CHECK(isValidCallsign("IU5HJU"));
  CHECK(!isValidCallsign(""));
  CHECK(!isValidCallsign("IU5 HJU"));
}

static void testParityRecovery() {
  // Gruppo con lunghezze diverse, START e END, per ogni indice mancante.
  Packet d[4] = {makeData(5, 0, 0, kFlagStart), makeData(6, 12, 10), makeData(7, 5, 20), makeData(8, 3, 30, kFlagEnd)};
  ParityAccumulator acc;
  for (auto& p : d) acc.add(p);
  CHECK(acc.count() == 4);
  Packet par;
  acc.build(par, 0xBEEF, kModeCodec2_1200, 9);
  CHECK(par.isParity() && par.payload[0] == 4);

  // La parita' sopravvive alla serializzazione.
  uint8_t buf[kMaxWireBytes];
  const size_t n = encodePacket(par, buf, sizeof(buf));
  Packet par2;
  CHECK(decodePacket(buf, n, par2));

  for (size_t miss = 0; miss < 4; miss++) {
    const Packet* g[4];
    for (size_t i = 0; i < 4; i++) g[i] = (i == miss) ? nullptr : &d[i];
    Packet rec;
    CHECK(recoverFromParity(par2, g, 4, rec));
    CHECK(rec.seq == d[miss].seq);
    CHECK(rec.session == 0xBEEF);
    CHECK(rec.flags == (d[miss].flags & kFlagsParityMask));
    CHECK(samePayload(rec, d[miss]));
  }

  // Due mancanti o nessuno: non recuperabile.
  const Packet* two[4] = {nullptr, nullptr, &d[2], &d[3]};
  Packet rec;
  CHECK(!recoverFromParity(par2, two, 4, rec));
  const Packet* none[4] = {&d[0], &d[1], &d[2], &d[3]};
  CHECK(!recoverFromParity(par2, none, 4, rec));
}

static void testPartialGroupParity() {
  Packet d[2] = {makeData(10, 12, 3), makeData(11, 7, 4, kFlagEnd)};
  ParityAccumulator acc;
  acc.add(d[0]);
  acc.add(d[1]);
  Packet par;
  acc.build(par, 1, kModeCodec2_1200, 12);
  const Packet* g[2] = {&d[0], nullptr};
  Packet rec;
  CHECK(recoverFromParity(par, g, 2, rec));
  CHECK(rec.seq == 11 && (rec.flags & kFlagEnd) && samePayload(rec, d[1]));
}

int main() {
  testRoundTrip();
  testRejects();
  testParityRecovery();
  testPartialGroupParity();
  TEST_MAIN_END();
}
