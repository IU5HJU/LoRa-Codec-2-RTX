// Comandi del sysop: firma, contatore, nominativo, risposta, spegnimento a tempo e limite di trasmissione continua.
#include <cstring>

#include "packet.h"
#include "sysop.h"
#include "testing.h"

using namespace lcr;

static const uint8_t KEY[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const uint8_t KEY2[16] = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
typedef SysopAuth::Verdict V;

static Packet overAir(const Packet& p) {
  uint8_t b[kMaxWireBytes];
  const size_t n = encodePacket(p, b, sizeof(b));
  CHECK(n > 0);
  Packet q;
  CHECK(decodePacket(b, n, q));
  return q;
}

static SysopAuth makeAuth() {
  SysopAuth a;
  a.setKey(KEY);
  a.setSysop("IZ0SOP");
  return a;
}

static void testCodec() {
  uint8_t b[kMaxWireBytes];
  const Packet c = makeSysopCommand(0x1234, "IZ0SOP", KEY, {kSysopOffMinutes, 30, 7});
  CHECK(encodePacket(c, b, sizeof(b)) == 5 + 1 + 6 + kSysopCommandBytes);
  const Packet r = makeSysopReply(0x1234, "IZ0RPT", KEY, {kSysopOff, kSysopOk, 9, 0x01000000});
  CHECK(encodePacket(r, b, sizeof(b)) == 5 + 1 + 6 + kSysopReplyBytes);
  Packet bad = c;
  bad.payloadLen = 10;
  CHECK(encodePacket(bad, b, sizeof(b)) == 0);
  bad = c;
  bad.callsign[0] = 0;
  CHECK(encodePacket(bad, b, sizeof(b)) == 0);
  Packet q;
  const size_t n = encodePacket(c, b, sizeof(b));
  CHECK(!decodePacket(b, n - 1, q));
}

static void testAccept() {
  SysopAuth a = makeAuth();
  SysopCommand cmd;
  CHECK(a.check(overAir(makeSysopCommand(1, "IZ0SOP", KEY, {kSysopOffMinutes, 30, 5})), cmd) == V::Ok);
  CHECK(cmd.code == kSysopOffMinutes && cmd.param == 30 && cmd.counter == 5 && a.lastCounter() == 5);
}

static void testRejects() {
  SysopAuth a = makeAuth();
  SysopCommand cmd;
  // chiave diversa
  CHECK(a.check(overAir(makeSysopCommand(1, "IZ0SOP", KEY2, {kSysopOff, 0, 1})), cmd) == V::BadSignature);
  // altro nominativo (con la chiave giusta)
  CHECK(a.check(overAir(makeSysopCommand(1, "IZ0AAA", KEY, {kSysopOff, 0, 1})), cmd) == V::WrongCallsign);
  // pacchetto modificato in volo: parametro cambiato, sessione cambiata, nominativo dopo la firma
  Packet p = makeSysopCommand(1, "IZ0SOP", KEY, {kSysopOffMinutes, 1, 2});
  p.payload[2] = 99;
  CHECK(a.check(overAir(p), cmd) == V::BadSignature);
  p = makeSysopCommand(1, "IZ0SOP", KEY, {kSysopOff, 0, 2});
  p.session = 2;
  CHECK(a.check(overAir(p), cmd) == V::BadSignature);
  p = makeSysopCommand(1, "IZ0SOP", KEY, {kSysopOff, 0, 2});
  p.payload[7] ^= 1;  // un bit della firma
  CHECK(a.check(overAir(p), cmd) == V::BadSignature);
  CHECK(a.lastCounter() == 0 && a.rejected() == 5);
  // risposta o pacchetti d'altro tipo: non e' un comando
  CHECK(a.check(overAir(makeSysopReply(1, "IZ0SOP", KEY, {kSysopOff, 0, 3, 0})), cmd) == V::NotCommand);
  Packet voice;
  CHECK(a.check(voice, cmd) == V::NotCommand);
  // senza chiave o senza sysop: tutto rifiutato, anche una firma corretta
  SysopAuth none;
  none.setSysop("IZ0SOP");
  CHECK(none.check(overAir(makeSysopCommand(1, "IZ0SOP", KEY, {kSysopStatus, 0, 1})), cmd) == V::NoKey);
  SysopAuth nosop;
  nosop.setKey(KEY);
  CHECK(nosop.check(overAir(makeSysopCommand(1, "IZ0SOP", KEY, {kSysopStatus, 0, 1})), cmd) == V::NoKey);
}

static void testReplay() {
  SysopAuth a = makeAuth();
  SysopCommand cmd;
  const Packet c5 = makeSysopCommand(1, "IZ0SOP", KEY, {kSysopOff, 0, 5});
  CHECK(a.check(overAir(c5), cmd) == V::Ok);
  CHECK(a.check(overAir(c5), cmd) == V::Replay);  // stessa registrazione riascoltata
  CHECK(a.check(overAir(makeSysopCommand(2, "IZ0SOP", KEY, {kSysopOn, 0, 4})), cmd) == V::Replay);  // contatore piu' basso
  CHECK(a.check(overAir(makeSysopCommand(3, "IZ0SOP", KEY, {kSysopOn, 0, 6})), cmd) == V::Ok);
  CHECK(a.lastCounter() == 6);
  // contatore ripristinato dalla memoria dopo un riavvio
  SysopAuth b = makeAuth();
  b.setLastCounter(6);
  CHECK(b.check(overAir(c5), cmd) == V::Replay);
}

static void testReply() {
  const Packet r = overAir(makeSysopReply(0x4321, "IZ0RPT", KEY, {kSysopOffMinutes, kSysopOk, 77, 0x01100005}));
  SysopReply out;
  CHECK(parseSysopReply(r, KEY, out) && out.code == kSysopOffMinutes && out.result == kSysopOk && out.counter == 77 && out.data == 0x01100005);
  CHECK(!parseSysopReply(r, KEY2, out));  // risposta falsa
  Packet f = r;
  f.payload[1] = kSysopOk + 1;
  CHECK(!parseSysopReply(f, KEY, out));
  // un comando non e' una risposta
  CHECK(!parseSysopReply(overAir(makeSysopCommand(1, "IZ0SOP", KEY, {kSysopStatus, 0, 1})), KEY, out));
}

static void testControl() {
  RepeaterControl c(2, 17, 10);
  RepeaterControl::Effects fx;
  uint32_t data = 0;
  CHECK(c.txAllowed(0) && c.apply({kSysopStatus, 0, 1}, 0, fx, data) == kSysopOk && data == (10u << 16));
  CHECK(c.apply({kSysopOff, 0, 2}, 1000, fx, data) == kSysopOk && fx.forceReply && !c.txAllowed(1000) && (data >> 24) == 1 && (data & 0xFFFF) == 0xFFFF);
  CHECK(!c.txAllowed(10000000));  // senza scadenza
  CHECK(c.apply({kSysopOn, 0, 3}, 2000, fx, data) == kSysopOk && c.txAllowed(2000) && !fx.forceReply);
  // a tempo
  CHECK(c.apply({kSysopOffMinutes, 2, 4}, 10000, fx, data) == kSysopOk && fx.forceReply && !c.txAllowed(10001) && (data & 0xFFFF) == 2);
  CHECK(!c.txAllowed(10000 + 119000) && c.txAllowed(10000 + 120000));
  CHECK(c.apply({kSysopOffMinutes, 0, 5}, 0, fx, data) == kSysopBadParam && c.txAllowed(200000));
  CHECK(c.apply({kSysopOffMinutes, kSysopMaxOffMinutes + 1, 6}, 0, fx, data) == kSysopBadParam && c.txAllowed(200000));
  // potenza
  CHECK(c.apply({kSysopPower, 14, 7}, 0, fx, data) == kSysopOk && fx.power == 14 && c.power() == 14);
  CHECK(c.apply({kSysopPower, 18, 8}, 0, fx, data) == kSysopBadParam && c.power() == 14 && fx.power == -1);
  CHECK(c.apply({kSysopPower, 1, 9}, 0, fx, data) == kSysopBadParam);
  // altri comandi e sconosciuto
  CHECK(c.apply({kSysopRestart, 0, 10}, 0, fx, data) == kSysopOk && fx.restart);
  CHECK(c.apply({kSysopIdentify, 0, 11}, 0, fx, data) == kSysopOk && fx.identify && !fx.restart);
  CHECK(c.apply({99, 0, 12}, 0, fx, data) == kSysopUnknown);
}

static void testSafetyLimit() {
  RepeaterControl c(2, 17, 10);
  // voce continua: pacchetti ogni 400 ms da 300 ms
  uint32_t t = 0;
  bool ok = true;
  while (ok && t < 120000) {
    ok = c.noteTx(t, 300, false);
    t += 400;
  }
  CHECK(!ok && t > 58000 && t < 62000 && !c.txAllowed(t) && c.safetyTrips() == 1);
  CHECK(c.txAllowed(t + RepeaterControl::kSafetyOffMs + 1000));  // si riaccende da sola
  // con una pausa la trasmissione non e' continua: nessuno scatto
  RepeaterControl d(2, 17, 10);
  t = 0;
  for (int i = 0; i < 100; i++) {
    CHECK(d.noteTx(t, 20000, false));
    t += 20000 + 5000;
  }
  CHECK(d.safetyTrips() == 0);
  // un messaggio di 139 s e' ammesso, uno di 200 s no
  RepeaterControl m(2, 17, 10);
  t = 0;
  ok = true;
  for (int i = 0; i < 41; i++) {
    ok = m.noteTx(t, 3400, true);
    t += 3480;
  }
  CHECK(ok && m.safetyTrips() == 0);
  for (int i = 0; i < 20 && ok; i++) {
    ok = m.noteTx(t, 3400, true);
    t += 3480;
  }
  CHECK(!ok);
  // spegnimento a tempo del sysop non deve essere accorciato da un comando di accensione: RIACCENDI lo toglie
  RepeaterControl::Effects fx;
  uint32_t data;
  m.apply({kSysopOn, 0, 1}, t, fx, data);
  CHECK(m.txAllowed(t));
}

int main() {
  testCodec();
  testAccept();
  testRejects();
  testReplay();
  testReply();
  testControl();
  testSafetyLimit();
  TEST_MAIN_END();
}
