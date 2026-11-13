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
  CHECK(q.complete() && !q.clean());
  CHECK(!q.perfect());
  r.framesOk = 300;
  r.framesBad = r.framesLost = 0;
  CHECK(r.perfect() && r.percentOk() == 100);
}

static void testPartial() {
  TestReport r;
  CHECK(r.percentOk() == 0 && !r.clean() && !r.complete());  // niente ricevuto
  r.framesOut = r.framesOk = 7;  // prova interrotta dopo 7 frame, tutti perfetti
  CHECK(r.percentOk() == 100 && r.clean() && !r.complete() && !r.perfect());
  r.framesOut = 10;
  r.framesOk = 9;
  r.framesLost = 1;
  CHECK(r.percentOk() == 90 && !r.clean());
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
  CHECK(s.b.note.reports[0].r.clean() && !s.b.note.reports[0].r.complete() && s.b.note.reports[0].r.percentOk() == 100);
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

// ---- Prova di potenza ----
static void testSweepLogic() {
  PowerSweep w;
  w.begin(17, 2, 10);
  TestReport ok;
  ok.framesOk = ok.framesOut = kSweepFrames;
  CHECK(w.active() && w.currentDbm() == 17 && w.restoreDbm() == 10);
  // 17..12 perfetti, 11 al 80%, 10 al 20%, poi niente: due passi muti di fila fermano la prova
  int dbm = 17;
  for (; dbm >= 12; dbm--) {
    CHECK(w.currentDbm() == dbm && w.record(true, ok));
  }
  TestReport r80 = ok;
  r80.framesOk = static_cast<uint16_t>(kSweepFrames * 8 / 10);
  CHECK(w.currentDbm() == 11 && w.record(true, r80));
  TestReport r20 = ok;
  r20.framesOk = static_cast<uint16_t>(kSweepFrames / 5);
  CHECK(w.currentDbm() == 10 && w.record(true, r20));
  CHECK(w.currentDbm() == 9 && w.record(false, TestReport()));
  CHECK(w.currentDbm() == 8 && !w.record(false, TestReport()) && !w.active());
  CHECK(w.steps() == 10 && w.step(6).percent == 80 && w.step(7).percent == 20 && !w.step(8).answered && !w.step(9).answered);
  CHECK(w.minPerfectDbm() == 12);   // dalla massima senza interruzioni
  CHECK(w.minAnsweredDbm() == 10);  // l'ultima che ha ancora risposto
  // fino al minimo se risponde sempre
  PowerSweep v;
  v.begin(5, 2, 5);
  while (v.active()) v.record(true, ok);
  CHECK(v.steps() == 4 && v.minPerfectDbm() == 2 && v.minAnsweredDbm() == 2);
  // mai risposta: due passi e fine, nessun minimo
  PowerSweep z;
  z.begin(17, 2, 17);
  z.record(false, TestReport());
  CHECK(!z.record(false, TestReport()) && z.steps() == 2 && z.minPerfectDbm() == -1 && z.minAnsweredDbm() == -1);
  // un buco in mezzo non conta per il minimo "pulito"
  PowerSweep h;
  h.begin(6, 2, 6);
  h.record(true, ok);
  h.record(true, r80);
  h.record(true, ok);
  CHECK(h.minPerfectDbm() == 6);
}

// La prova di potenza fra due stazioni: sotto 9 dBm l'altra scheda non sente piu' niente (un'antenna lontana), a 9-10 dBm perde dei pacchetti.
static void testSweepEndToEnd() {
  Sim s;
  s.a.node.setTestMode(true);
  s.b.node.setTestMode(true);
  s.air.drop = [&](int from, int idx) {
    (void)idx;
    if (from != 0) return false;  // le risposte di B tornano sempre
    const int p = s.a.radio.powers.empty() ? 17 : s.a.radio.powers.back();
    if (p < 9) return true;
    if (p < 11) return idx % 3 == 1;  // un pacchetto su tre
    return false;
  };
  s.run(100);
  CHECK(!s.a.node.powerSweepActive());
  CHECK(s.a.node.startPowerSweep(s.clock, 14, 2, 12));
  CHECK(s.a.node.powerSweepActive());
  s.run(120000);
  CHECK(!s.a.node.powerSweepActive() && s.a.note.sweepDone == 1 && !s.a.note.sweepAborted);
  // 14, 13, 12, 11 perfetti; 10 e 9 con dei buchi; 8 e 7 muti: la prova si ferma a 7
  const auto& st = s.a.note.sweepSteps;
  CHECK(st.size() == 8);
  for (size_t i = 0; i < st.size() && i < 4; i++) CHECK(st[i].answered && st[i].percent == 100 && st[i].dbm == 14 - static_cast<int>(i));
  CHECK(st.size() > 5 && st[4].dbm == 10 && st[4].answered && st[4].percent < 100 && st[4].percent > 0);
  CHECK(st.size() > 7 && !st[6].answered && !st[7].answered && st[7].dbm == 7);
  CHECK(s.a.note.sweepMinPerfect == 11 && s.a.note.sweepMinAnswered == 9);
  // la potenza torna a quella di prima, e ogni passo l'ha impostata
  CHECK(!s.a.radio.powers.empty() && s.a.radio.powers.back() == 12);
  CHECK(s.a.radio.powers.size() == st.size() + 1 && s.a.radio.powers[0] == 14 && s.a.radio.powers[3] == 11);
  // l'altra scheda ha ricevuto una prova per passo
  CHECK(s.b.note.reports.size() >= 6);
  // dopo la prova si puo' rifare una prova normale, di 12 s
  s.a.node.setTestMode(true);
  s.a.node.pttPressed(s.clock);
  s.run(16000);
  CHECK(s.b.note.reports.back().r.framesOut == kTestFrames);
}

static void testSweepRefusedAndAborted() {
  Sim s;
  s.run(100);
  CHECK(!s.a.node.startPowerSweep(s.clock, 17, 2, 10));  // prova non attiva
  s.a.node.setTestMode(true);
  s.b.node.setTestMode(true);
  CHECK(s.a.node.startPowerSweep(s.clock, 17, 2, 10));
  CHECK(!s.a.node.startPowerSweep(s.clock, 17, 2, 10));  // gia' in corso
  s.run(2000);
  CHECK(s.a.node.powerSweepActive());
  s.a.node.pttPressed(s.clock);  // un PTT ferma tutto e rimette la potenza
  CHECK(!s.a.node.powerSweepActive() && s.a.note.sweepDone == 1 && s.a.note.sweepAborted);
  CHECK(s.a.radio.powers.back() == 10);
  // spegnere la prova ferma la prova di potenza
  Sim t;
  t.a.node.setTestMode(true);
  t.b.node.setTestMode(true);
  CHECK(t.a.node.startPowerSweep(t.clock, 17, 2, 9));
  t.run(500);
  t.a.node.setTestMode(false);
  t.run(5000);  // la prova in corso finisce di trasmettere, poi la prova di potenza si ferma
  CHECK(!t.a.node.powerSweepActive() && t.a.note.sweepAborted && t.a.radio.powers.back() == 9);
  // senza nessuno dall'altra parte: due passi e fine, nessun minimo
  Sim u;
  u.a.node.setTestMode(true);
  CHECK(u.a.node.startPowerSweep(u.clock, 17, 2, 17));
  u.run(60000);
  CHECK(u.a.note.sweepSteps.size() == 2 && u.a.note.sweepMinPerfect == -1 && u.a.note.sweepMinAnswered == -1 && !u.a.node.powerSweepActive());
}

int main() {
  testPattern();
  testReportCodec();
  testPartial();
  testRxAccumulator();
  testEndToEnd();
  testNoReplyWhenTestOff();
  testLossesAreCounted();
  testReleaseEarly();
  testVoiceUnaffected();
  testOnlyOwnReply();
  testSweepLogic();
  testSweepEndToEnd();
  testSweepRefusedAndAborted();
  TEST_MAIN_END();
}
