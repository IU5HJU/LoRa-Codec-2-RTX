#include <cstdlib>
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

static void testCodec3200() {
  // Modo 4: frame da 16 byte, 12 per pacchetto = 192 byte
  Packet p;
  p.session = 0x1234;
  p.seq = 2;
  p.mode = kModeCodec2_3200;
  p.payloadLen = static_cast<uint8_t>(12 * 16);
  for (size_t i = 0; i < p.payloadLen; i++) p.payload[i] = static_cast<uint8_t>(i * 3 + 1);
  uint8_t buf[kMaxWireBytes];
  const size_t n = encodePacket(p, buf, sizeof(buf));
  CHECK(n == 5 + 192);
  Packet q;
  CHECK(decodePacket(buf, n, q));
  CHECK(q.mode == kModeCodec2_3200 && q.payloadLen == 192 && samePayload(p, q));
  // Lunghezza non multipla di 16: scartato; 6 byte (un frame del 1200) non e' valido nel modo 4
  CHECK(!decodePacket(buf, n - 1, q));
  uint8_t shortBuf[5 + 6];
  memcpy(shortBuf, buf, 5);
  memset(shortBuf + 5, 1, 6);
  CHECK(!decodePacket(shortBuf, sizeof(shortBuf), q));
  // Parita' su pacchetti da 16 byte per frame
  Packet d[4];
  for (int i = 0; i < 4; i++) {
    d[i] = p;
    d[i].seq = static_cast<uint8_t>(5 + i);
    d[i].payloadLen = static_cast<uint8_t>((i == 3 ? 5 : 12) * 16);
    for (size_t k = 0; k < d[i].payloadLen; k++) d[i].payload[k] = static_cast<uint8_t>(i * 40 + k * 7);
  }
  ParityAccumulator acc;
  for (auto& x : d) acc.add(x);
  Packet par;
  acc.build(par, 0x1234, kModeCodec2_3200, 9);
  const size_t pn = encodePacket(par, buf, sizeof(buf));
  Packet par2;
  CHECK(decodePacket(buf, pn, par2));
  for (size_t miss = 0; miss < 4; miss++) {
    const Packet* g[4];
    for (size_t i = 0; i < 4; i++) g[i] = (i == miss) ? nullptr : &d[i];
    Packet rec;
    CHECK(recoverFromParity(par2, g, 4, rec));
    CHECK(samePayload(rec, d[miss]));
  }
}

static void testPosition() {
  Packet p = makeData(0, 0, 1, kFlagStart);
  strcpy(p.callsign, "IU5HJU");
  p.position.valid = true;
  p.position.fromGps = true;
  p.position.latE7 = 437696000;
  p.position.lonE7 = 112558000;
  uint8_t buf[kMaxWireBytes];
  const size_t n = encodePacket(p, buf, sizeof(buf));
  CHECK(n == 5 + 1 + 6 + kPositionBytes);  // START con nominativo e posizione, nessun frame
  Packet q;
  CHECK(decodePacket(buf, n, q));
  CHECK(q.flags == (kFlagStart | kFlagCallsign | kFlagPosition));
  CHECK(q.hasPosition() && q.position.fromGps);
  CHECK(std::abs(q.position.latE7 - p.position.latE7) < 120 && std::abs(q.position.lonE7 - p.position.lonE7) < 240);
  CHECK(strcmp(q.callsign, "IU5HJU") == 0);

  // Con frame audio dopo il blocco posizione.
  Packet d = makeData(0, 2, 5, kFlagStart);
  strcpy(d.callsign, "IU5HJU");
  d.position = p.position;
  const size_t m = encodePacket(d, buf, sizeof(buf));
  CHECK(m == 5 + 1 + 6 + kPositionBytes + 12);
  CHECK(decodePacket(buf, m, q) && q.hasPosition() && samePayload(d, q));

  // Senza posizione il pacchetto e' quello di prima.
  Packet r = makeData(0, 0, 1, kFlagStart);
  strcpy(r.callsign, "IU5HJU");
  CHECK(encodePacket(r, buf, sizeof(buf)) == 5 + 1 + 6);
  CHECK(decodePacket(buf, 12, q) && !q.hasPosition() && (q.flags & kFlagPosition) == 0);

  // La posizione richiede il nominativo; coordinate fuori limite non si trasmettono.
  Packet nocall = makeData(0, 0, 1, kFlagStart);
  nocall.position = p.position;
  CHECK(encodePacket(nocall, buf, sizeof(buf)) == 0);
  Packet bad = p;
  bad.position.latE7 = kMaxLatE7 + 1;
  CHECK(encodePacket(bad, buf, sizeof(buf)) == 0);

  // In ricezione: flag senza il blocco, senza nominativo, o su un pacchetto di parita' = scartato.
  const size_t full = encodePacket(p, buf, sizeof(buf));
  CHECK(!decodePacket(buf, full - 1, q));  // blocco troncato
  uint8_t cut[kMaxWireBytes];
  memcpy(cut, buf, full);
  cut[4] &= static_cast<uint8_t>(~kFlagCallsign);
  CHECK(!decodePacket(cut, full, q));
  memcpy(cut, buf, full);
  cut[4] = kFlagParity | kFlagPosition;
  CHECK(!decodePacket(cut, full, q));
  // Un pacchetto della versione 1 non si capisce piu'.
  memcpy(cut, buf, full);
  cut[0] = static_cast<uint8_t>((1 << 4) | kModeCodec2_1200);
  CHECK(!decodePacket(cut, full, q));
}

static void testTotal() {
  Packet p = makeData(0, 0, 1, kFlagStart);
  strcpy(p.callsign, "IU5HJU");
  p.totalPackets = 80;
  uint8_t buf[kMaxWireBytes];
  const size_t n = encodePacket(p, buf, sizeof(buf));
  CHECK(n == 5 + 1 + 6 + 1);  // START con nominativo e totale
  Packet q;
  CHECK(decodePacket(buf, n, q));
  CHECK(q.hasTotal() && q.totalPackets == 80 && (q.flags & kFlagTotal) && !q.hasPosition());
  // Insieme alla posizione: prima la posizione, poi il totale.
  p.position.valid = true;
  p.position.latE7 = 437696000;
  p.position.lonE7 = 112558000;
  const size_t m = encodePacket(p, buf, sizeof(buf));
  CHECK(m == 5 + 1 + 6 + kPositionBytes + 1);
  CHECK(buf[m - 1] == 80);
  CHECK(decodePacket(buf, m, q) && q.hasPosition() && q.totalPackets == 80);
  // Senza il totale il pacchetto e' quello di prima.
  Packet r = makeData(0, 0, 1, kFlagStart);
  strcpy(r.callsign, "IU5HJU");
  CHECK(encodePacket(r, buf, sizeof(buf)) == 12 && decodePacket(buf, 12, q) && !q.hasTotal());
  // Il totale si dichiara solo nel pacchetto START.
  Packet notStart = makeData(3, 12, 1, 0);
  notStart.totalPackets = 80;
  CHECK(encodePacket(notStart, buf, sizeof(buf)) == 0);
  // In ricezione: flag senza il byte, totale zero, o senza START = scartato.
  const size_t full = encodePacket(p, buf, sizeof(buf));
  uint8_t cut[kMaxWireBytes];
  CHECK(!decodePacket(buf, full - 1, q));  // totale mancante
  memcpy(cut, buf, full);
  cut[full - 1] = 0;
  CHECK(!decodePacket(cut, full, q));
  memcpy(cut, buf, full);
  cut[4] &= static_cast<uint8_t>(~kFlagStart);
  CHECK(!decodePacket(cut, full, q));
  memcpy(cut, buf, full);
  cut[4] = kFlagParity | kFlagTotal;
  CHECK(!decodePacket(cut, full, q));
  // Il bit 7 e' il flag "ripetuto" (ripetitore): un pacchetto dati con quel bit e' valido e il bit si conserva.
  memcpy(cut, buf, full);
  cut[4] |= kFlagRelayed;
  CHECK(decodePacket(cut, full, q) && (q.flags & kFlagRelayed));
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
  bad[0] = 0x11;                           // versione 1 (vecchia): scartata
  CHECK(!decodePacket(bad, n, q));
  memcpy(bad, buf, n);
  bad[4] = kFlagParity | kFlagStart;      // parita' con START: combinazione non valida
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
  testCodec3200();
  testPosition();
  testTotal();
  testRejects();
  testParityRecovery();
  testPartialGroupParity();
  TEST_MAIN_END();
}
