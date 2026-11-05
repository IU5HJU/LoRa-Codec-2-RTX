// Prova di collegamento: sequenza nota, confronto bit per bit, risposta automatica.
#include <cstring>

#include "fakes.h"
#include "link_test.h"
#include "testing.h"
#include "voice_node.h"

using namespace lcr;

static uint16_t nextSession() {
  static uint16_t s = 500;
  return s++;
}

struct Station {
  Station(fake::Air& air, int id, const char* cs, const uint32_t& clock)
      : radio(air, id), mic(clock), node(makeCfg(cs), radio, codec, mic, spk, note) {}
  static NodeConfig makeCfg(const char* cs) {
    NodeConfig c;
    strcpy(c.callsign, cs);
    c.newSessionId = nextSession;
    return c;
  }
  fake::Radio radio;
  fake::Codec codec;
  fake::Mic mic;
  fake::Speaker spk;
  fake::Notifier note;
  VoiceNode node;
};

struct Sim {
  fake::Air air;
  uint32_t clock = 0;
  Station a{air, 0, "IU5HJU", clock};
  Station b{air, 1, "IZ5ABC", clock};
  void run(uint32_t ms) {
    const uint32_t end = clock + ms;
    while (clock < end) {
      a.node.tick(clock);
      b.node.tick(clock);
      clock += 10;
    }
  }
};

static void testPattern() {
  uint8_t f1[kFrameBytes], f2[kFrameBytes], f3[kFrameBytes], f4[kFrameBytes];
  testFramePattern(7, 0, f1);
  testFramePattern(7, 0, f2);
  testFramePattern(7, 1, f3);
  testFramePattern(8, 0, f4);
  CHECK(memcmp(f1, f2, kFrameBytes) == 0);  // deterministico
  CHECK(memcmp(f1, f3, kFrameBytes) != 0);  // cambia con la posizione
  CHECK(memcmp(f1, f4, kFrameBytes) != 0);  // e con la sessione
  // tutti i frame di una prova sono distinti
  bool distinct = true;
  uint8_t prev[kTestFrames][kFrameBytes];
  for (uint32_t i = 0; i < kTestFrames; i++) {
    testFramePattern(1234, i, prev[i]);
    for (uint32_t j = 0; j < i; j++)
      if (memcmp(prev[i], prev[j], kFrameBytes) == 0) distinct = false;
  }
  CHECK(distinct);
}

static void testReportCodec() {
  TestReport r;
  r.framesOut = 300;
  r.framesOk = 296;
  r.framesBad = 1;
  r.framesLost = 3;
  r.recovered = 5;
  r.rssiDbm = -92;
  r.snrDb = -7;
  r.snrValid = true;
  uint8_t b[kTestReportBytes];
  encodeTestReport(r, b);
  TestReport q;
  CHECK(decodeTestReport(b, sizeof(b), q));
  CHECK(q.framesOut == 300 && q.framesOk == 296 && q.framesBad == 1 && q.framesLost == 3);
  CHECK(q.recovered == 5 && q.rssiDbm == -92 && q.snrDb == -7 && q.snrValid);
  CHECK(!decodeTestReport(b, sizeof(b) - 1, q));
  CHECK(q.percentOk() == 99);  // 296/300
  CHECK(!q.perfect());
  r.framesOk = 300;
  r.framesBad = r.framesLost = 0;
  CHECK(r.perfect() && r.percentOk() == 100);
}

static void testRxAccumulator() {
  LinkTestRx rx;
  rx.begin(42);
  uint8_t f[kFrameBytes];
  for (uint32_t i = 0; i < 10; i++) {
    testFramePattern(42, i, f);
    if (i == 3) f[0] ^= 0x10;           // un bit sbagliato
    if (i == 6) { rx.onLost(); continue; }
    rx.onFrame(f);
  }
  const TestReport r = rx.report(2);
  CHECK(r.framesOut == 10 && r.framesOk == 8 && r.framesBad == 1 && r.framesLost == 1 && r.recovered == 2);
}

// La prova completa tra due stazioni, con la risposta automatica.
static void testEndToEnd() {
  Sim s;
  s.a.node.setTestMode(true);
  s.b.node.setTestMode(true);
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(16000);  // 12 s di prova + riproduzione e risposta
  CHECK(s.a.node.state() == VoiceNode::State::Idle);
  CHECK(s.b.node.state() == VoiceNode::State::Idle);
  CHECK(s.a.mic.produced() == 0);  // il microfono non serve
  CHECK(s.b.spk.played.empty());   // niente audio: i frame di prova non sono voce

  // Misura locale su B.
  CHECK(s.b.note.reports.size() == 1);
  const auto& lb = s.b.note.reports[0];
  CHECK(!lb.fromRemote && lb.callsign == "IU5HJU");
  CHECK(lb.r.framesOut == kTestFrames && lb.r.framesOk == kTestFrames && lb.r.framesBad == 0 && lb.r.framesLost == 0);
  CHECK(lb.r.perfect() && lb.r.percentOk() == 100);
  CHECK(s.b.note.rxEnds == 0);  // non e' una ricezione di voce

  // Risposta arrivata ad A, con lo stesso risultato e il nominativo di B.
  CHECK(s.a.note.reports.size() == 1);
  const auto& ra = s.a.note.reports[0];
  CHECK(ra.fromRemote && ra.callsign == "IZ5ABC");
  CHECK(ra.r.framesOk == kTestFrames && ra.r.perfect());
}

static void testNoReplyWhenTestOff() {
  Sim s;
  s.a.node.setTestMode(true);  // solo A ha la prova attiva
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(16000);
  CHECK(s.b.note.reports.size() == 1 && s.b.note.reports[0].r.perfect());  // B misura comunque
  CHECK(s.b.spk.played.empty());                                           // e non suona rumore
  CHECK(s.a.note.reports.empty());                                         // ma non risponde
  CHECK(s.air.sent[1] == 0);
}

static void testLossesAreCounted() {
  Sim s;
  s.a.node.setTestMode(true);
  s.b.node.setTestMode(true);
  // Pacchetto 2 (un dato) perso: la parita' lo recupera. Pacchetti 6 e 7 persi nello stesso gruppo: 24 frame persi.
  s.air.drop = [](int from, int idx) { return from == 0 && (idx == 2 || idx == 6 || idx == 7); };
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(16000);
  CHECK(s.b.note.reports.size() == 1);
  const TestReport& r = s.b.note.reports[0].r;
  CHECK(r.framesBad == 0);                 // quello che arriva e' intatto
  CHECK(r.framesLost == 24);               // due pacchetti da 12 frame non recuperabili
  CHECK(r.framesOk == kTestFrames - 24);
  CHECK(r.recovered >= 1);
  CHECK(!r.perfect());
  CHECK(s.a.note.reports.size() == 1 && s.a.note.reports[0].r.framesLost == 24);  // lo stesso risultato torna ad A
}

static void testReleaseEarly() {
  Sim s;
  s.a.node.setTestMode(true);
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(3000);
  s.a.node.pttReleased(s.clock);  // rilascio anticipato
  s.run(8000);
  CHECK(s.a.node.state() == VoiceNode::State::Idle);
  CHECK(s.b.note.reports.size() == 1);
  CHECK(s.b.note.reports[0].r.framesBad == 0 && s.b.note.reports[0].r.framesLost == 0);
  CHECK(s.b.note.reports[0].r.framesOut > 0 && s.b.note.reports[0].r.framesOut < kTestFrames);
}

static void testVoiceUnaffected() {
  // Senza prova attiva il PTT trasmette voce come sempre e B la riproduce.
  Sim s;
  s.b.node.setTestMode(true);  // B in prova, A no: A manda voce
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(3000);
  s.a.node.pttReleased(s.clock);
  s.run(6000);
  CHECK(!s.b.spk.played.empty());
  CHECK(s.b.note.reports.empty());
  CHECK(s.b.note.rxEnds == 1);
}

static void testOnlyOwnReply() {
  // Una risposta con un'altra sessione si ignora.
  Sim s;
  s.a.node.setTestMode(true);
  s.b.node.setTestMode(true);
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(16000);
  CHECK(s.a.note.reports.size() == 1);
  // Nessuna seconda risposta anche dopo altro tempo.
  s.run(5000);
  CHECK(s.a.note.reports.size() == 1);
}

int main() {
  testPattern();
  testReportCodec();
  testRxAccumulator();
  testEndToEnd();
  testNoReplyWhenTestOff();
  testLossesAreCounted();
  testReleaseEarly();
  testVoiceUnaffected();
  testOnlyOwnReply();
  TEST_MAIN_END();
}
