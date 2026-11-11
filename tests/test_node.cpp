// Test end-to-end della macchina a stati con due nodi su un canale simulato.
#include <memory>

#include <cstdlib>

#include "fakes.h"
#include "testing.h"
#include "voice_node.h"

using namespace lcr;

static uint16_t nextSession() {
  static uint16_t s = 100;
  return s++;
}

static std::vector<std::string> g_meshLog;
static void testMeshLog(const char* m) { g_meshLog.push_back(m); }

struct Station {
  Station(fake::Air& air, int id, const char* cs, const uint32_t& clock, uint8_t profile = kProfileStandard, uint32_t extraGapMs = 0)
      : radio(air, id), codec(frameBytesFor(profileFor(profile).codecMode)), mic(clock), node(makeCfg(cs, profile, extraGapMs), radio, codec, mic, spk, note) {}
  static NodeConfig makeCfg(const char* cs, uint8_t profile = kProfileStandard, uint32_t extraGapMs = 0) {
    NodeConfig c;
    strcpy(c.callsign, cs);
    c.newSessionId = nextSession;
    c.profile = profile;
    c.extraGapMs = extraGapMs;
    snprintf(c.mcCall, sizeof(c.mcCall), "%s-99", cs ? cs : "");  // escursione MeshCom: nominativo-SSID
    c.mcHw = 12;
    c.meshLog = testMeshLog;
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
  Station a;
  Station b;
  explicit Sim(uint8_t profile = kProfileStandard, uint32_t extraGapMs = 0)
      : a(air, 0, "IU5HJU", clock, profile, extraGapMs), b(air, 1, "IZ5ABC", clock, profile, extraGapMs) {}
  // Avanza il tempo di ms millisecondi a passi di 10 ms.
  void run(uint32_t ms) {
    const uint32_t end = clock + ms;
    while (clock < end) {
      a.node.tick(clock);
      b.node.tick(clock);
      clock += 10;
    }
  }
};

static std::vector<int> seq(int n) {
  std::vector<int> v;
  for (int i = 1; i <= n; i++) v.push_back(i);
  return v;
}

static void testPacketSpacing() {
  // Parita' e dati non devono partire attaccati: il ricevitore ha bisogno di tempo per leggere la radio.
  Sim s;
  s.air.clock = &s.clock;
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(7000);
  s.a.node.pttReleased(s.clock);
  s.run(3000);
  const auto& t = s.air.sentAt[0];
  CHECK(t.size() >= 12);  // START, dati, parita', END...
  for (size_t i = 1; i < t.size(); i++) CHECK(t[i] - t[i - 1] >= kInterPacketGapMs);
  CHECK(s.a.node.state() == VoiceNode::State::Idle);  // la trasmissione si chiude comunque
}

static void testOneTurn() {
  Sim s;
  s.run(100);
  s.a.node.pttPressed(s.clock);
  CHECK(s.a.node.state() == VoiceNode::State::Idle);  // la trasmissione parte solo se il PTT resta premuto
  s.run(kPttClickMaxMs + 50);
  CHECK(s.a.node.state() == VoiceNode::State::Transmitting);
  s.run(3000);
  s.a.node.pttReleased(s.clock);
  s.run(6000);

  const int n = static_cast<int>(s.a.mic.produced());
  CHECK(n >= 80 && n <= 88);  // ~3,35 s: il microfono e' bufferizzato dalla pressione, non dalla partenza della TX
  CHECK(s.b.spk.played == seq(n));
  CHECK(s.b.note.callsign == "IU5HJU");
  CHECK(s.b.note.rxStarts == 1 && s.b.note.rxEnds == 1);
  CHECK(s.a.note.txStarts == 1 && s.a.note.txEnds == 1 && !s.a.note.limitHit);
  CHECK(s.a.node.state() == VoiceNode::State::Idle);
  CHECK(s.b.node.state() == VoiceNode::State::Idle);
  CHECK(s.a.spk.played.empty());  // chi trasmette non riproduce
}

// Premi e rilascia il PTT dopo 'holdMs'; poi attende 'gapMs'.
static void click(Sim& s, uint32_t holdMs, uint32_t gapMs) {
  s.a.node.pttPressed(s.clock);
  s.run(holdMs);
  s.a.node.pttReleased(s.clock);
  s.run(gapMs);
}

static void testTripleClickTogglesTestMode() {
  Sim s;
  s.run(100);
  CHECK(!s.a.node.testMode());
  click(s, 100, 150);
  click(s, 100, 150);
  CHECK(!s.a.node.testMode());  // dopo due clic non cambia ancora nulla
  s.a.node.pttPressed(s.clock);  // terzo clic
  CHECK(s.a.node.testMode());
  CHECK(s.a.note.testModeChanges.size() == 1 && s.a.note.testModeChanges[0]);
  s.run(120);
  s.a.node.pttReleased(s.clock);
  s.run(1000);
  CHECK(s.a.node.state() == VoiceNode::State::Idle);
  CHECK(s.air.sent[0] == 0 && s.a.note.txStarts == 0);  // nessuna trasmissione per i clic

  // Altri tre clic la disattivano.
  click(s, 100, 150);
  click(s, 100, 150);
  s.a.node.pttPressed(s.clock);
  CHECK(!s.a.node.testMode());
  CHECK(s.a.note.testModeChanges.size() == 2 && !s.a.note.testModeChanges[1]);
  s.a.node.pttReleased(s.clock);
  s.run(1000);
  CHECK(s.air.sent[0] == 0);
}

static void testSlowClicksDoNothing() {
  Sim s;
  s.run(100);
  const uint32_t slow = kPttClickGapMs + 200;
  click(s, 100, slow);  // pausa troppo lunga tra un clic e l'altro: il conteggio riparte
  click(s, 100, slow);
  click(s, 100, slow);
  CHECK(!s.a.node.testMode());
  // Due clic veloci, pausa, due clic veloci: mai tre di seguito.
  click(s, 100, 150);
  click(s, 100, slow);
  click(s, 100, 150);
  click(s, 100, slow);
  CHECK(!s.a.node.testMode());
  CHECK(s.air.sent[0] == 0 && s.a.note.testModeChanges.empty());
}

static void testShortTapDoesNotTransmit() {
  Sim s;
  s.run(100);
  click(s, 100, 1500);
  CHECK(s.air.sent[0] == 0 && s.a.note.txStarts == 0 && s.a.note.denied == 0);
  CHECK(s.a.node.state() == VoiceNode::State::Idle);
}

static void testClicksWorkWithoutCallsign() {
  // La prova si attiva anche con N0CALL (si riceve e si misura, ma non si trasmette).
  fake::Air air;
  uint32_t clock = 0;
  Station n0(air, 0, "N0CALL", clock);
  auto step = [&](uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10, clock += 10) n0.node.tick(clock); };
  for (int i = 0; i < 2; i++) {
    n0.node.pttPressed(clock);
    step(100);
    n0.node.pttReleased(clock);
    step(150);
  }
  n0.node.pttPressed(clock);
  CHECK(n0.node.testMode());
  n0.node.pttReleased(clock);
  CHECK(n0.note.denied == 0);
}

static void testLongPressAfterClicks() {
  // Due clic, una pausa lunga, poi un PTT tenuto premuto: e' una trasmissione, non cambia il modo prova.
  Sim s;
  s.run(100);
  s.a.node.setTestMode(true);
  click(s, 100, 100);
  click(s, 100, kPttClickGapMs + 200);
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 100);
  CHECK(s.a.node.state() == VoiceNode::State::Transmitting);
  s.a.node.pttReleased(s.clock);
  s.run(5000);
  CHECK(s.a.node.testMode() && s.a.note.testModeChanges.empty());
  // Terza pressione subito dopo due clic, anche se tenuta premuta: e' il terzo clic e non trasmette.
  Sim t;
  t.run(100);
  click(t, 100, 100);
  click(t, 100, 100);
  t.a.node.pttPressed(t.clock);
  t.run(kPttClickMaxMs + 200);
  CHECK(t.a.node.testMode() && t.a.node.state() == VoiceNode::State::Idle && t.air.sent[0] == 0);
}

static void testReplyAfterWhite() {
  Sim s;
  s.a.node.pttPressed(s.clock);
  s.run(4000);
  s.a.node.pttReleased(s.clock);
  s.run(6000);  // l'operatore B aspetta, poi risponde
  const int n1 = static_cast<int>(s.a.mic.produced());
  CHECK(s.b.spk.played == seq(n1));

  s.b.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50);
  CHECK(s.b.node.state() == VoiceNode::State::Transmitting);
  s.run(2000);
  s.b.node.pttReleased(s.clock);
  s.run(6000);
  const int n2 = static_cast<int>(s.b.mic.produced());
  CHECK(n2 > 40);
  CHECK(s.a.spk.played == seq(n2));
  CHECK(s.a.note.callsign == "IZ5ABC");

  // Terzo turno: di nuovo A, la sessione precedente non interferisce.
  s.a.spk.played.clear();
  s.b.spk.played.clear();
  s.a.node.pttPressed(s.clock);
  s.run(1500);
  s.a.node.pttReleased(s.clock);
  s.run(6000);
  CHECK(s.b.spk.played == seq(static_cast<int>(s.a.mic.produced())));
  CHECK(s.b.note.rxEnds == 2);
}

static void testPttDenied() {
  Sim s;
  // Canale occupato da un'altra stazione.
  s.air.busy = true;
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50);
  CHECK(s.a.node.state() == VoiceNode::State::Idle);
  CHECK(s.a.note.denied == 1 && s.a.note.lastDeny == DenyReason::ChannelBusy);
  s.air.busy = false;

  // B sta ricevendo A: il suo PTT e' rifiutato.
  s.a.node.pttPressed(s.clock);
  s.run(2500);
  CHECK(s.b.node.state() == VoiceNode::State::Receiving);
  s.b.node.pttPressed(s.clock);
  CHECK(s.b.node.state() == VoiceNode::State::Receiving);
  CHECK(s.b.note.denied == 1 && s.b.note.lastDeny == DenyReason::Receiving);
  s.a.node.pttReleased(s.clock);
  s.run(6000);
  CHECK(s.b.node.state() == VoiceNode::State::Idle);
}

static void testTurnLimit() {
  Sim s;
  s.a.node.pttPressed(s.clock);
  s.run(40000);  // PTT tenuto 40 s
  CHECK(s.a.node.state() == VoiceNode::State::Idle);  // fermato a 30 s
  CHECK(s.a.note.limitHit);
  s.a.node.pttReleased(s.clock);
  CHECK(s.a.node.state() == VoiceNode::State::Idle);  // il rilascio tardivo non riparte
  s.run(6000);
  const int n = static_cast<int>(s.a.mic.produced());
  CHECK(n == static_cast<int>(kMaxTurnFrames));
  CHECK(s.b.spk.played == seq(n));
}

static void testPlaceholderCallsignBlocksTx() {
  // Con N0CALL (o NOCALL) la scheda non trasmette, ma riceve normalmente.
  fake::Air air;
  uint32_t clock = 0;
  Station silent(air, 0, "N0CALL", clock);
  Station talker(air, 1, "IU5HJU", clock);
  silent.node.pttPressed(clock);
  for (uint32_t t = 0; t < kPttClickMaxMs + 50; t += 10, clock += 10) silent.node.tick(clock);
  CHECK(silent.node.state() == VoiceNode::State::Idle);
  CHECK(silent.note.denied == 1 && silent.note.lastDeny == DenyReason::NoCallsign);
  CHECK(silent.note.txStarts == 0 && air.sent[0] == 0);
  Station other(air, 0, "NOCALL", clock);
  other.node.pttPressed(clock);
  for (uint32_t t = 0; t < kPttClickMaxMs + 50; t += 10, clock += 10) other.node.tick(clock);
  CHECK(other.note.lastDeny == DenyReason::NoCallsign && air.sent[0] == 0);

  // Riceve la voce di un'altra stazione.
  talker.node.pttPressed(clock);
  for (int i = 0; i < 600; i++, clock += 10) {
    silent.node.tick(clock);
    talker.node.tick(clock);
    if (i == 250) talker.node.pttReleased(clock);
  }
  CHECK(silent.spk.played.size() > 20);
  CHECK(silent.note.callsign == "IU5HJU");
  CHECK(isPlaceholderCallsign("N0CALL") && isPlaceholderCallsign("NOCALL"));
  CHECK(!isPlaceholderCallsign("IU5HJU") && !isPlaceholderCallsign("N0CALL1") && !isPlaceholderCallsign(""));
}

static void testLossOnAir() {
  Sim s;
  // Si perde un pacchetto dati per gruppo: la parita' lo recupera.
  s.air.drop = [](int from, int idx) { return from == 0 && (idx == 2 || idx == 7 || idx == 12); };
  s.a.node.pttPressed(s.clock);
  s.run(8000);
  s.a.node.pttReleased(s.clock);
  s.run(6000);
  const int n = static_cast<int>(s.a.mic.produced());
  CHECK(s.b.spk.played == seq(n));
  CHECK(s.b.note.recovered == 3 && s.b.note.lostFrames == 0);
}

static void testRecordThenSend() {
  // Registra e poi trasmetti: niente radio finche' si registra, trasmissione 1 s dopo il rilascio, stesso audio.
  Sim s;
  s.air.clock = &s.clock;
  s.a.node.setRecordMode(true);
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50);
  CHECK(s.a.node.state() == VoiceNode::State::Transmitting && s.a.node.recPhase() == VoiceNode::RecPhase::Recording);
  s.run(3000);
  CHECK(s.air.sent[0] == 0);  // si registra soltanto
  s.a.node.pttReleased(s.clock);
  const uint32_t releaseAt = s.clock;
  CHECK(s.a.node.recPhase() == VoiceNode::RecPhase::Waiting);
  s.run(kRecSendDelayMs - 100);
  CHECK(s.air.sent[0] == 0);  // ancora in attesa
  s.run(8000);
  CHECK(!s.air.sentAt[0].empty() && s.air.sentAt[0][0] >= releaseAt + kRecSendDelayMs);
  const int n = static_cast<int>(s.a.mic.produced());
  CHECK(n >= 78 && n <= 90);
  CHECK(s.b.spk.played == seq(n));  // la voce arriva intera e in ordine
  CHECK(s.b.note.callsign == "IU5HJU");
  CHECK(s.a.note.txStarts == 1 && s.a.note.txEnds == 1 && !s.a.note.limitHit);
  CHECK(s.a.node.state() == VoiceNode::State::Idle && s.a.node.recPhase() == VoiceNode::RecPhase::None);
  CHECK((s.a.note.recPhases == std::vector<int>{1, 2, 3, 0}));
  for (size_t i = 1; i < s.air.sentAt[0].size(); i++) CHECK(s.air.sentAt[0][i] - s.air.sentAt[0][i - 1] >= kInterPacketGapMs);
}

static void testRecordTurnLimit() {
  // PTT tenuto oltre i 30 s: la registrazione si ferma da sola e il messaggio parte comunque.
  Sim s;
  s.a.node.setRecordMode(true);
  s.a.node.pttPressed(s.clock);
  s.run(40000);
  CHECK(s.a.note.limitHit);
  s.a.node.pttReleased(s.clock);  // rilascio tardivo: non cambia niente
  s.run(40000);
  CHECK(static_cast<int>(s.a.mic.produced()) == static_cast<int>(kMaxTurnFrames));
  CHECK(s.b.spk.played == seq(static_cast<int>(kMaxTurnFrames)));
  CHECK(s.a.node.state() == VoiceNode::State::Idle);
}

static void testRecordModeOffByDefault() {
  Sim s;
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 400);
  CHECK(s.a.node.recPhase() == VoiceNode::RecPhase::None && s.air.sent[0] > 0);  // trasmette subito, come prima
  s.a.node.pttReleased(s.clock);
  s.run(3000);
}

// Ricevitore B: un clic breve sul PTT apre il messaggio in casella e lo riproduce (poi si cancella da solo).
static void listenB(Sim& s, int frames) {
  s.b.node.pttPressed(s.clock);
  s.run(100);
  s.b.node.pttReleased(s.clock);
  s.run(static_cast<uint32_t>(frames) * kFrameMs + 1500);
}

// Profilo Messaggio: ogni pacchetto dura circa 3,4 s in aria (extraGapMs lo simula, la radio finta e' istantanea).
static constexpr uint32_t kMsgAirMs = 3400;

static void testMessageProfile() {
  Sim s(kProfileMessage, kMsgAirMs);
  s.air.clock = &s.clock;
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50);
  CHECK(s.a.node.state() == VoiceNode::State::Transmitting && s.a.node.recPhase() == VoiceNode::RecPhase::Recording);
  s.run(3000);
  s.a.node.pttReleased(s.clock);
  const uint32_t releaseAt = s.clock;
  s.run(kRecSendDelayMs - 100);
  CHECK(s.air.sent[0] == 0);  // si registra, poi si aspetta: niente in aria
  s.run(20000);
  CHECK(!s.air.sentAt[0].empty() && s.air.sentAt[0][0] >= releaseAt + kRecSendDelayMs);
  CHECK(s.air.sent[0] >= 3);
  // A meta' della trasmissione chi riceve ha gia' dei pacchetti ma NON riproduce: aspetta la fine del messaggio.
  CHECK(s.b.node.state() == VoiceNode::State::Receiving);
  CHECK(s.b.spk.played.empty() && s.b.note.playbackStarts == 0);
  // Intervalli tra pacchetti vicini al tempo in aria: il ricevitore non chiude la sessione per silenzio.
  CHECK(s.b.note.rxEnds == 0);
  s.run(90000);
  const int n = static_cast<int>(s.a.mic.produced());
  CHECK(n >= 70 && n <= 90);
  // A ricezione conclusa il messaggio NON parte da solo: e' in casella e aspetta il clic sul PTT.
  CHECK(s.b.spk.played.empty() && s.b.note.playbackStarts == 0);
  CHECK(s.b.note.received.size() == 1 && !s.b.note.received[0].isText && s.b.note.received[0].callsign == "IU5HJU");
  CHECK(s.b.node.inbox().kind() == Inbox::Kind::Voice && !s.b.node.messageOpen());
  listenB(s, n);
  CHECK(s.b.spk.played == seq(n));  // tutto il messaggio, in ordine
  CHECK(s.b.note.opened.size() == 1 && s.b.note.closed == 1 && !s.b.node.inbox().has());  // ascoltato: cancellato
  CHECK(s.b.note.rxStarts == 1 && s.b.note.rxEnds == 1);
  CHECK(s.b.note.callsign == "IU5HJU");
  CHECK(s.a.node.state() == VoiceNode::State::Idle && s.b.node.state() == VoiceNode::State::Idle);
  CHECK(s.a.note.txEnds == 1 && !s.a.note.limitHit);
  // Spenta o accesa che sia, la registrazione resta attiva nel profilo Messaggio
  s.a.node.setRecordMode(false);
  CHECK(s.a.node.recordMode());
}

static void testQualityProfile() {
  Sim s(kProfileQuality);
  s.air.clock = &s.clock;
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(3000);
  s.a.node.pttReleased(s.clock);
  s.run(4000);
  const int n = static_cast<int>(s.a.mic.produced());
  CHECK(n >= 70 && n <= 90);
  CHECK(s.b.spk.played == seq(n));  // tutti i frame da 16 byte, in ordine e con il contenuto intatto
  CHECK(s.b.note.callsign == "IU5HJU");
  CHECK(s.b.note.rxEnds == 1);
  // Nessuna comunicazione con il profilo Standard: modo diverso, il pacchetto e' ignorato
  Sim x(kProfileStandard);
  x.air.clock = &x.clock;
  x.run(100);
  x.a.node.pttPressed(x.clock);
  x.run(1500);
  x.a.node.pttReleased(x.clock);
  x.run(2000);
  CHECK(x.b.spk.played.size() > 5);
  CHECK(profileFor(kProfileQuality).codecMode == kModeCodec2_3200 && !profileFor(kProfileQuality).message);
}

static void testMessageEndLost() {
  // Pacchetto END e relativa parita' persi: dopo 12 s senza altro il messaggio si riproduce comunque.
  int total = 0;
  {
    Sim ref(kProfileMessage, kMsgAirMs);
    ref.a.node.pttPressed(ref.clock);
    ref.run(kPttClickMaxMs + 50 + 3000);
    ref.a.node.pttReleased(ref.clock);
    ref.run(110000);
    total = ref.air.sent[0];
  }
  CHECK(total >= 6);
  Sim s(kProfileMessage, kMsgAirMs);
  s.air.drop = [total](int from, int idx) { return from == 0 && idx >= total - 2; };
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50 + 3000);
  s.a.node.pttReleased(s.clock);
  s.run(110000);
  CHECK(s.b.node.state() == VoiceNode::State::Idle);
  CHECK(s.b.node.inbox().kind() == Inbox::Kind::Voice);
  listenB(s, static_cast<int>(s.b.node.inbox().frames()));
  CHECK(!s.b.spk.played.empty());
  // Quello che era arrivato si sente in ordine, senza dover aspettare la parita' o l'END
  int last = 0;
  for (int f : s.b.spk.played) {
    if (f > 0) {
      CHECK(f == last + 1);
      last = f;
    }
  }
  CHECK(last >= static_cast<int>(s.a.mic.produced()) - 14);  // l'ultimo pacchetto da 12 frame (e al massimo qualche frame) manca
}

static void testMessageProfileRules() {
  Sim s(kProfileMessage, kMsgAirMs);
  s.a.node.setTestMode(true);
  CHECK(!s.a.node.testMode());  // niente prova di collegamento nel profilo Messaggio
  // Tre clic veloci non la attivano
  s.run(100);
  for (int i = 0; i < 3; i++) {
    s.a.node.pttPressed(s.clock);
    s.run(100);
    s.a.node.pttReleased(s.clock);
    s.run(150);
  }
  CHECK(!s.a.node.testMode() && s.a.note.testModeChanges.empty());
  // Un messaggio molto lungo si ferma al limite del profilo (30 s di voce)
  s.a.node.pttPressed(s.clock);
  s.run(40000);
  CHECK(s.a.node.recPhase() != VoiceNode::RecPhase::Recording);  // la registrazione si e' fermata da sola a 30 s
  s.a.node.pttReleased(s.clock);
  s.run(450000);
  CHECK(s.air.sent[0] > 60 && s.air.sent[0] < static_cast<int>(kTxSeqLimit));  // circa 80 pacchetti: sotto il limite di sequenza
  CHECK(s.a.note.limitHit);  // e il limite e' stato segnalato
  CHECK(static_cast<uint32_t>(s.a.mic.produced()) == profileFor(kProfileMessage).maxTurnMs / kFrameMs);
  listenB(s, static_cast<int>(profileFor(kProfileMessage).maxTurnMs / kFrameMs));
  CHECK(s.b.spk.played == seq(static_cast<int>(profileFor(kProfileMessage).maxTurnMs / kFrameMs)));
}

static Position qth(double lat, double lon) {
  Position p;
  p.valid = true;
  p.latE7 = static_cast<int32_t>(lat * 1e7);
  p.lonE7 = static_cast<int32_t>(lon * 1e7);
  return p;
}

static void turn(Sim& s) {
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50 + 2000);
  s.a.node.pttReleased(s.clock);
  s.run(5000);
}

static void testPositionOffByDefault() {
  // Anche con una posizione disponibile, se la trasmissione non e' attivata non parte nulla.
  Sim s;
  s.a.node.setOwnPosition(qth(43.7696, 11.2558), false);
  turn(s);
  CHECK(s.b.note.rxStarts == 1 && s.b.note.callsign == "IU5HJU");
  CHECK(s.b.note.positions == 0);
}

static void testPositionInStartOnly() {
  Sim s;
  s.a.node.setOwnPosition(qth(43.7696, 11.2558), true);
  turn(s);
  CHECK(s.b.note.rxStarts == 1 && s.b.note.rxEnds == 1);
  CHECK(s.b.note.positions == 1);  // una sola volta per ricezione, anche se il nominativo si ripete
  CHECK(s.b.note.position.valid && !s.b.note.position.fromGps);
  CHECK(std::abs(s.b.note.position.latE7 - 437696000) < 120 && std::abs(s.b.note.position.lonE7 - 112558000) < 240);
  // Nei pacchetti trasmessi la posizione c'e' solo in quello con START.
  int withPos = 0, withStart = 0, withCall = 0;
  for (const auto& raw : s.air.log[0]) {
    Packet p;
    if (!decodePacket(raw.data(), raw.size(), p)) continue;
    if (p.hasPosition()) withPos++;
    if (p.flags & kFlagStart) withStart++;
    if (p.hasCallsign()) withCall++;
    if (p.hasPosition()) CHECK(p.flags & kFlagStart);
  }
  CHECK(withPos == 1 && withStart == 1 && withCall >= 2);
  // La voce passa uguale.
  const int n = static_cast<int>(s.a.mic.produced());
  CHECK(s.b.spk.played == seq(n));
}

static void testPositionNotSentWithoutFix() {
  Sim s;
  s.a.node.setOwnPosition(Position(), true);  // abilitata ma nessuna posizione (GPS senza fix, QTH non impostato)
  turn(s);
  CHECK(s.b.note.rxStarts == 1 && s.b.note.positions == 0);
}

static void testExpectedPackets() {
  // Il totale calcolato in anticipo coincide con i pacchetti davvero prodotti, per ogni lunghezza e con/senza parita'.
  for (int fec = 0; fec < 2; fec++) {
    for (size_t frames = 0; frames <= 750; frames++) {
      TxSession tx("IU5HJU", 1, fec != 0);
      tx.setExpectedTotal(TxSession::expectedPackets(frames, fec != 0));
      uint8_t f[kFrameBytes] = {1, 2, 3, 4, 5, 6};
      size_t count = 0;
      Packet p;
      uint8_t declared = 0;
      tx.begin();
      for (size_t i = 0; i < frames; i++) {
        tx.pushFrame(f);
        while (tx.popPacket(p)) {
          if (count++ == 0) declared = p.totalPackets;
        }
      }
      tx.finish();
      while (tx.popPacket(p)) {
        if (count++ == 0) declared = p.totalPackets;
      }
      if (count != TxSession::expectedPackets(frames, fec != 0) || declared != count) {
        CHECK(count == TxSession::expectedPackets(frames, fec != 0));
        CHECK(declared == count);
        return;
      }
    }
  }
  CHECK(TxSession::expectedPackets(0, true) == 3);   // START, END, parita'
  CHECK(TxSession::expectedPackets(12, false) == 3); // START, un pacchetto pieno, END senza frame
  // Lunghezze non dichiarabili in un byte: nessun totale.
  TxSession big("IU5HJU", 1, true);
  big.setExpectedTotal(300);
  Packet p;
  big.begin();
  CHECK(big.popPacket(p) && p.totalPackets == 0);
}

static void testMessageProgress() {
  Sim s(kProfileMessage, kMsgAirMs);
  s.air.clock = &s.clock;
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50 + 3000);
  s.a.node.pttReleased(s.clock);
  s.run(120000);
  CHECK(s.b.note.received.size() == 1 && s.a.note.txEnds == 1);
  // Il pacchetto START dichiara il totale, uguale ai pacchetti davvero trasmessi.
  Packet start;
  CHECK(decodePacket(s.air.log[0][0].data(), s.air.log[0][0].size(), start));
  CHECK(start.hasTotal() && start.totalPackets == s.air.log[0].size());
  // Trasmissione: da 1% a 100%, mai indietro, un avviso per pacchetto.
  const auto& tx = s.a.note.txPercents;
  CHECK(tx.size() == s.air.log[0].size() + 1);  // 1% all'avvio dell'invio, poi un avviso per pacchetto
  CHECK(!tx.empty() && tx.front() == 1 && tx.back() == 100);
  for (size_t i = 1; i < tx.size(); i++) CHECK(tx[i] >= tx[i - 1]);
  // Ricezione: stessa cosa.
  const auto& rx = s.b.note.rxPercents;
  CHECK(!rx.empty() && rx.front() == 1 && rx.back() == 100);
  for (size_t i = 1; i < rx.size(); i++) CHECK(rx[i] > rx[i - 1]);  // si notifica solo quando sale
  // Chi trasmette non vede la percentuale di ricezione e viceversa.
  CHECK(s.a.note.rxPercents.empty() && s.b.note.txPercents.empty());
}

static void testProgressLostTail() {
  // Se l'ultimo pacchetto e la sua parita' si perdono la ricezione chiude comunque a 100% all'inizio della riproduzione.
  Sim s(kProfileMessage, kMsgAirMs);
  s.air.clock = &s.clock;
  s.air.drop = [](int from, int idx) { return from == 0 && idx >= 12; };  // si perde tutto dopo il dodicesimo pacchetto
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50 + 8000);
  s.a.node.pttReleased(s.clock);
  s.run(200000);
  const auto& rx = s.b.note.rxPercents;
  CHECK(!rx.empty() && rx.front() >= 1 && rx.back() == 100);
  for (size_t i = 1; i < rx.size(); i++) CHECK(rx[i] > rx[i - 1]);
}

static void testNoProgressInStandard() {
  // La voce in tempo reale non conosce la lunghezza in anticipo: nessuna percentuale, ne' in trasmissione ne' in ricezione.
  Sim s;
  s.run(100);
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50 + 3000);
  s.a.node.pttReleased(s.clock);
  s.run(6000);
  CHECK(s.b.note.rxEnds == 1);
  CHECK(s.a.note.txPercents.empty() && s.b.note.rxPercents.empty());
  Packet start;
  CHECK(decodePacket(s.air.log[0][0].data(), s.air.log[0][0].size(), start) && !start.hasTotal());
}

static void testTextMessage() {
  Sim s;
  s.air.clock = &s.clock;
  s.run(100);
  CHECK(s.a.node.sendText("Arrivo tra 10 minuti", s.clock));
  CHECK(s.a.node.state() == VoiceNode::State::Transmitting);
  s.run(3000);
  CHECK(s.a.node.state() == VoiceNode::State::Idle && s.a.note.txEnds == 1);
  CHECK(s.air.sent[0] == 2);  // due copie
  // B: un solo messaggio (la seconda copia si scarta), in casella, non aperto
  CHECK(s.b.note.received.size() == 1 && s.b.note.received[0].isText && s.b.note.received[0].callsign == "IU5HJU");
  CHECK(s.b.node.inbox().kind() == Inbox::Kind::Text && strcmp(s.b.node.inbox().text(), "Arrivo tra 10 minuti") == 0);
  CHECK(!s.b.node.messageOpen() && s.b.spk.played.empty());
  // clic: si apre e resta in mostra; un altro clic lo chiude e lo cancella
  s.b.node.pttPressed(s.clock);
  s.run(100);
  s.b.node.pttReleased(s.clock);
  CHECK(s.b.node.messageOpen() && s.b.note.opened.size() == 1 && s.b.note.opened[0]);
  s.run(5000);
  CHECK(s.b.node.messageOpen() && s.b.node.inbox().has());
  s.b.node.pttPressed(s.clock);
  s.b.node.pttReleased(s.clock);
  CHECK(!s.b.node.messageOpen() && !s.b.node.inbox().has() && s.b.note.closed == 1);
  CHECK(s.b.node.state() == VoiceNode::State::Idle && s.b.note.txStarts == 0);  // il clic non trasmette

  // un testo nuovo (altra sessione) cancella il vecchio
  CHECK(s.a.node.sendText("Uno", s.clock));
  s.run(3000);
  CHECK(s.a.node.sendText("Due", s.clock));
  s.run(3000);
  CHECK(s.b.note.received.size() == 3 && strcmp(s.b.node.inbox().text(), "Due") == 0);

  // testo non valido o troppo lungo: rifiutato, niente in aria
  const int sent = s.air.sent[0];
  CHECK(!s.a.node.sendText("", s.clock) && !s.a.node.sendText("a\tb", s.clock));
  std::string big(151, 'x');
  CHECK(!s.a.node.sendText(big.c_str(), s.clock));
  s.run(1000);
  CHECK(s.air.sent[0] == sent);
  // 150 caratteri vanno
  big.resize(150);
  CHECK(s.a.node.sendText(big.c_str(), s.clock));
  s.run(3000);
  CHECK(s.b.node.inbox().textLen() == 150);
}

static void testTextRules() {
  Sim s;
  s.run(100);
  // il PTT lungo durante la lettura chiude soltanto il messaggio, non trasmette
  CHECK(s.a.node.sendText("ciao", s.clock));
  s.run(3000);
  s.b.node.pttPressed(s.clock);
  s.run(100);
  s.b.node.pttReleased(s.clock);
  CHECK(s.b.node.messageOpen());
  s.b.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 100);
  s.b.node.pttReleased(s.clock);
  CHECK(!s.b.node.messageOpen() && s.b.note.txStarts == 0 && s.b.node.state() == VoiceNode::State::Idle);
}

static void testVoiceMessageStopAndReplace() {
  Sim s(kProfileMessage, kMsgAirMs);
  s.air.clock = &s.clock;
  s.run(100);
  auto sendVoice = [&]() {
    s.a.node.pttPressed(s.clock);
    s.run(kPttClickMaxMs + 50 + 2000);
    s.a.node.pttReleased(s.clock);
    s.run(100000);
  };
  sendVoice();
  CHECK(s.b.note.received.size() == 1 && s.b.node.inbox().kind() == Inbox::Kind::Voice);
  const size_t first = s.b.node.inbox().frames();
  // un secondo messaggio non ascoltato cancella il primo (una sola casella)
  sendVoice();
  CHECK(s.b.note.received.size() == 2 && s.b.node.inbox().kind() == Inbox::Kind::Voice);
  // si apre e a meta' si preme il PTT: la riproduzione si ferma e il messaggio si cancella
  s.b.node.pttPressed(s.clock);
  s.run(100);
  s.b.node.pttReleased(s.clock);
  s.run(1000);
  CHECK(s.b.node.messageOpen() && !s.b.spk.played.empty());
  const size_t partial = s.b.spk.played.size();
  CHECK(partial < s.b.node.inbox().frames());
  s.b.node.pttPressed(s.clock);
  s.b.node.pttReleased(s.clock);
  CHECK(!s.b.node.messageOpen() && !s.b.node.inbox().has() && s.b.note.closed == 1);
  s.run(3000);
  CHECK(s.b.spk.played.size() == partial);  // niente piu' audio
  (void)first;
}

// ---- Escursione MeshCom: testi compatibili con la rete a pacchetto ----

static std::vector<uint8_t> meshFrame(const char* path, const char* dest, const char* payload, uint32_t id, uint8_t type = mc::kTypeText,
                                      uint8_t flags = mc::kFlagMesh, uint8_t hop = 3) {
  mc::Frame f;
  f.type = type;
  f.msgId = id;
  f.flags = flags;
  f.maxHop = hop;
  strcpy(f.path, path);
  strcpy(f.dest, dest);
  strcpy(f.payload, payload);
  f.hw = 3;
  uint8_t out[256];
  const size_t n = mc::encode(f, out, sizeof(out));
  CHECK(n > 0);
  return std::vector<uint8_t>(out, out + n);
}

// Le due schede passano alla rete di testo: A scrive, B ascolta. Finestra di 60 s.
static constexpr uint32_t kWin = 60000;

static void testMeshcomSendReceive() {
  Sim s;  // profilo Standard: l'escursione parte da un profilo voce
  s.air.clock = &s.clock;
  s.run(100);
  CHECK(!s.a.radio.meshcom && !s.a.node.meshcomActive());
  CHECK(s.b.node.startMeshcom(nullptr, nullptr, s.clock, kWin));  // B: solo ascolto
  CHECK(s.a.node.startMeshcom("Ciao dal progetto", "IZ5ABC-99", s.clock, kWin));
  CHECK(s.a.radio.meshcom && s.a.node.meshcomActive() && s.b.radio.meshcom && s.a.note.meshcomEvents.size() == 1 && s.a.note.meshcomEvents[0]);
  CHECK(s.a.node.state() == VoiceNode::State::Transmitting);
  s.run(2000);
  CHECK(s.a.node.state() == VoiceNode::State::Idle && s.a.note.txEnds == 1);
  CHECK(s.air.sent[0] == 1);  // un solo invio, niente seconda copia
  mc::Frame f;
  CHECK(mc::decode(s.air.log[0][0].data(), s.air.log[0][0].size(), f) && f.fcsOk);
  CHECK(f.type == mc::kTypeText && strcmp(f.path, "IU5HJU-99") == 0 && strcmp(f.dest, "IZ5ABC-99") == 0 && strcmp(f.payload, "Ciao dal progetto") == 0);
  CHECK(f.maxHop == 4 && f.flags == mc::kFlagMesh && f.hw == 12 && f.msgId != 0 && !f.hasFwTrailer);
  // B lo riceve nella casella, con mittente nominativo-SSID
  CHECK(s.b.note.received.size() == 1 && s.b.note.received[0].isText && s.b.note.received[0].callsign == "IU5HJU-99");
  CHECK(s.b.node.inbox().kind() == Inbox::Kind::Text && strcmp(s.b.node.inbox().text(), "Ciao dal progetto") == 0);
  CHECK(strcmp(s.b.node.inbox().callsign(), "IU5HJU-99") == 0);
  // una copia ripetuta da un nodo (stesso ID, percorso piu' lungo) non e' un messaggio nuovo; nemmeno la nostra eco
  s.air.inbox[1].push_back(meshFrame("IU5HJU-99,IU5CCC-12", "IZ5ABC-99", "Ciao dal progetto", f.msgId, mc::kTypeText, 0x90, 2));
  s.air.inbox[0].push_back(meshFrame("IU5HJU-99,IU5CCC-12", "IZ5ABC-99", "Ciao dal progetto", f.msgId));
  s.run(500);
  CHECK(s.b.note.received.size() == 1 && s.a.note.received.empty());
  // clic sul PTT: si apre e si legge; niente trasmissione
  s.b.node.pttPressed(s.clock);
  s.run(100);
  s.b.node.pttReleased(s.clock);
  CHECK(s.b.node.messageOpen() && s.b.note.txStarts == 0);
}

static void testMeshcomWindow() {
  Sim s;
  s.air.clock = &s.clock;
  s.run(100);
  CHECK(s.a.node.startMeshcom("Ciao", "22251", s.clock, kWin));
  s.run(2000);
  // destinatario = un gruppo: parte come un testo qualunque
  mc::Frame f;
  CHECK(mc::decode(s.air.log[0][0].data(), s.air.log[0][0].size(), f) && strcmp(f.dest, "22251") == 0);
  // il tempo che resta scende e a fine finestra la radio torna da sola al profilo voce
  CHECK(s.a.node.meshcomRemainingMs(s.clock) > 50000 && s.a.node.meshcomRemainingMs(s.clock) <= kWin);
  s.run(30000);
  CHECK(s.a.node.meshcomActive() && s.a.node.meshcomRemainingMs(s.clock) > 20000);
  s.run(31000);
  CHECK(!s.a.node.meshcomActive() && !s.a.radio.meshcom && s.a.node.meshcomRemainingMs(s.clock) == 0);
  CHECK(s.a.note.meshcomEvents.size() == 2 && !s.a.note.meshcomEvents[1]);
  CHECK(s.a.radio.meshcomSwitches == 2);
  // dopo il rientro la voce del progetto funziona come prima (e il PTT trasmette)
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 50 + 1500);
  s.a.node.pttReleased(s.clock);
  s.run(3000);
  CHECK(s.b.note.rxStarts == 1 && !s.b.spk.played.empty());

  // un secondo testo nella stessa finestra: la allunga, senza riportare la radio al profilo e riprenderla
  Sim t;
  t.run(100);
  CHECK(t.a.node.startMeshcom("Uno", "222", t.clock, kWin));
  t.run(2000);
  CHECK(t.a.node.startMeshcom("Due", "IZ5ABC-99", t.clock, kWin));
  t.run(2000);
  CHECK(t.a.radio.meshcomSwitches == 1 && t.air.sent[0] == 2);
  CHECK(t.a.node.meshcomRemainingMs(t.clock) > 50000);
  // fine anticipata
  t.a.node.endMeshcom();
  CHECK(!t.a.node.meshcomActive() && !t.a.radio.meshcom && t.a.radio.meshcomSwitches == 2);
  t.a.node.endMeshcom();  // di nuovo: niente
  CHECK(t.a.radio.meshcomSwitches == 2);
}

static void testMeshcomRefused() {
  Sim s;
  s.run(100);
  // senza destinatario non parte e non si resta sulla rete
  CHECK(!s.a.node.startMeshcom("ciao", "", s.clock, kWin));
  CHECK(s.a.note.lastDeny == DenyReason::NoDestination && !s.a.node.meshcomActive() && !s.a.radio.meshcom && s.air.sent[0] == 0);
  CHECK(s.a.radio.meshcomSwitches == 2 && s.a.note.meshcomEvents.size() == 2);  // acceso e subito riportato com'era
  // canale occupato: stessa cosa
  s.air.busy = true;
  CHECK(!s.a.node.startMeshcom("ciao", "222", s.clock, kWin));
  CHECK(s.a.note.lastDeny == DenyReason::ChannelBusy && !s.a.node.meshcomActive() && !s.a.radio.meshcom);
  s.air.busy = false;
  // la radio non passa ai parametri della rete: niente escursione
  s.a.radio.failMeshcom = true;
  CHECK(!s.a.node.startMeshcom("ciao", "222", s.clock, kWin) && !s.a.node.meshcomActive());
  s.a.radio.failMeshcom = false;
  // testo non valido (graffe, vuoto)
  CHECK(!s.a.node.startMeshcom("a{b", "222", s.clock, kWin) && !s.a.node.meshcomActive());
  CHECK(!s.a.node.startMeshcom("", "222", s.clock, kWin) && !s.a.node.meshcomActive());
  // durante una ricezione o una trasmissione non si parte
  s.a.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 100);
  CHECK(s.a.node.state() == VoiceNode::State::Transmitting);
  CHECK(!s.a.node.startMeshcom("ciao", "222", s.clock, kWin) && !s.a.node.meshcomActive());
  s.a.node.pttReleased(s.clock);
  s.run(3000);
  // senza nominativo reale (N0CALL) non si trasmette
  NodeConfig c = Station::makeCfg("N0CALL", kProfileStandard);
  fake::Air air;
  uint32_t clock = 0;
  fake::Radio radio(air, 0);
  fake::Codec codec(kFrameBytes);
  fake::Mic mic(clock);
  fake::Speaker spk;
  fake::Notifier note;
  VoiceNode node(c, radio, codec, mic, spk, note);
  CHECK(!node.startMeshcom("ciao", "222", 0, kWin) && note.lastDeny == DenyReason::NoCallsign && !radio.meshcom && radio.meshcomSwitches == 0);
}

static void testMeshcomFilters() {
  Sim s;
  s.run(100);
  CHECK(s.b.node.startMeshcom(nullptr, nullptr, s.clock, 10 * kWin));
  uint32_t id = 0x100;
  auto push = [&](const std::vector<uint8_t>& v) {
    s.air.inbox[1].push_back(v);
    s.run(300);
  };
  // altri destinatari, tutti (*), gruppi, messaggi di servizio (ora di rete), posizioni, rapporti: niente in casella
  push(meshFrame("IU5XXX-12", "IU5YYY-9", "non per noi", id++));
  push(meshFrame("IU5XXX-12", "*", "ciao a tutti", id++));
  push(meshFrame("IU5XXX-12", "22251", "ciao al gruppo", id++));
  push(meshFrame("OE1XAR-22", "IZ5ABC-99", "{CET}2026-11-11 15:32:42", id++));
  push(meshFrame("IU5XXX-12", "*", "4340.68N/01103.20E#/B=100", id++, mc::kTypePos));
  push(meshFrame("IU5XXX-12", "H", "R5;11,111,3;", id++, mc::kTypeHey));
  // la conferma di un nostro testo (payload "NOSTRO:ackNNN") non si mostra
  push(meshFrame("IU5XXX-12", "IZ5ABC-99", "IZ5ABC-99:ack204", id++));
  CHECK(s.b.note.received.empty() && !s.b.node.inbox().has());
  // FCS sbagliata: scartato
  auto bad = meshFrame("IU5XXX-12", "IZ5ABC-99", "FCS rotta", id++);
  bad[10] ^= 0x01;
  push(bad);
  CHECK(s.b.note.received.empty());
  // un testo per noi: si accetta; maiuscole e minuscole nel destinatario indifferenti
  push(meshFrame("IU5XXX-12", "iz5abc-99", "Questo si", id++));
  CHECK(s.b.note.received.size() == 1 && strcmp(s.b.node.inbox().text(), "Questo si") == 0);
  CHECK(strcmp(s.b.node.inbox().callsign(), "IU5XXX-12") == 0);
  // il mittente e' il primo del percorso, anche con piu' nodi
  push(meshFrame("IU5XXX-12,IU5AAA-12,IU5BBB-9", "IZ5ABC-99", "Via nodi", id++));
  CHECK(strcmp(s.b.node.inbox().callsign(), "IU5XXX-12") == 0 && strcmp(s.b.node.inbox().text(), "Via nodi") == 0);
  // caratteri non ASCII e graffe nel testo: sostituiti con '?'; testo lungo: tagliato a 150
  push(meshFrame("IU5XXX-12", "IZ5ABC-99", "caff\xc3\xa8 {x} ok", id++));
  CHECK(strcmp(s.b.node.inbox().text(), "caff?? ?x? ok") == 0);
  std::string longText(190, 'a');
  push(meshFrame("IU5XXX-12", "IZ5ABC-99", longText.c_str(), id++));
  CHECK(s.b.node.inbox().textLen() == kMaxTextChars);
  // PTT lungo durante l'escursione: niente voce, avviso "solo testo"
  s.b.node.pttPressed(s.clock);
  s.run(kPttClickMaxMs + 100);
  s.b.node.pttReleased(s.clock);
  CHECK(s.b.node.state() == VoiceNode::State::Idle && s.b.note.lastDeny == DenyReason::TextOnly && s.b.note.txStarts == 0);
  // un pacchetto vocale del progetto non entra durante l'escursione (altro formato)
  Packet p;
  p.mode = kModeCodec2_1200;
  p.session = 5;
  p.flags = kFlagStart | kFlagEnd | kFlagCallsign;
  strcpy(p.callsign, "IU5HJU");
  uint8_t wire[kMaxWireBytes];
  const size_t n = encodePacket(p, wire, sizeof(wire));
  s.air.inbox[1].push_back(std::vector<uint8_t>(wire, wire + n));
  s.run(500);
  CHECK(s.b.note.rxStarts == 0 && s.b.node.state() == VoiceNode::State::Idle);
  // i testi tradizionali (modo 7 del progetto) non partono durante l'escursione
  CHECK(!s.b.node.sendText("ciao", s.clock));
}

static bool logHas(const char* part) {
  for (const auto& l : g_meshLog) {
    if (l.find(part) != std::string::npos) return true;
  }
  return false;
}

static void testMeshcomAck() {
  Sim s;
  s.air.clock = &s.clock;
  s.run(100);
  CHECK(s.b.node.startMeshcom(nullptr, nullptr, s.clock, 10 * kWin));
  // testo con richiesta di conferma "{123": il testo si mostra senza, e dopo un attimo parte "MITTENTE:ack123"
  s.air.inbox[1].push_back(meshFrame("IU5XXX-12", "IZ5ABC-99", "Prova{123", 0x777));
  s.run(300);
  CHECK(s.b.note.received.size() == 1 && strcmp(s.b.node.inbox().text(), "Prova") == 0);
  CHECK(s.air.sent[1] == 0);  // non subito
  s.run(3000);
  CHECK(s.air.sent[1] == 1);
  mc::Frame ack;
  CHECK(mc::decode(s.air.log[1][0].data(), s.air.log[1][0].size(), ack) && ack.fcsOk);
  CHECK(strcmp(ack.path, "IZ5ABC-99") == 0 && strcmp(ack.dest, "IU5XXX-12") == 0 && strcmp(ack.payload, "IU5XXX-12:ack123") == 0);
  CHECK(ack.msgId != 0x777 && ack.maxHop == 4);
  CHECK(logHas("CONFERMA ack123 INVIATA"));
  // le copie ripetute dello stesso messaggio non fanno ripetere la conferma
  s.air.inbox[1].push_back(meshFrame("IU5XXX-12,IU5CCC-12", "IZ5ABC-99", "Prova{123", 0x777, mc::kTypeText, 0x91, 2));
  s.run(3000);
  CHECK(s.air.sent[1] == 1 && s.b.note.received.size() == 1);
  // il mittente ritrasmette dopo 40 s (la conferma si e' persa): si conferma di nuovo
  s.run(40000);
  s.air.inbox[1].push_back(meshFrame("IU5XXX-12", "IZ5ABC-99", "Prova{123", 0x777));
  s.run(3000);
  CHECK(s.air.sent[1] == 2 && s.b.note.received.size() == 1);
  // senza "{NNN" nessuna conferma
  s.air.inbox[1].push_back(meshFrame("IU5XXX-12", "IZ5ABC-99", "Senza conferma", 0x778));
  s.run(4000);
  CHECK(s.air.sent[1] == 2 && s.b.note.received.size() == 2);
  // canale occupato: la conferma aspetta e parte quando si libera
  s.air.busy = true;
  s.air.inbox[1].push_back(meshFrame("IU5XXX-12", "IZ5ABC-99", "Altro{9}", 0x779));  // "{9}" non e' un numero in coda: nessuna conferma
  s.air.inbox[1].push_back(meshFrame("IU5XXX-12", "IZ5ABC-99", "Altro{9", 0x77A));
  s.run(3000);
  CHECK(s.air.sent[1] == 2);
  s.air.busy = false;
  s.run(1500);
  CHECK(s.air.sent[1] == 3);
}

static void testMeshcomGroups() {
  Sim s;
  s.run(100);
  const char* groups[] = {"22251", "222", "*", "IU5ABC-9", ""};  // solo i numeri sono gruppi
  s.b.node.setMeshcomGroups(groups, 5);
  CHECK(s.b.node.startMeshcom(nullptr, nullptr, s.clock, 10 * kWin));
  uint32_t id = 0x300;
  auto push = [&](const std::vector<uint8_t>& v) {
    s.air.inbox[1].push_back(v);
    s.run(300);
  };
  g_meshLog.clear();
  // risposta nel gruppo a cui abbiamo scritto: si vede, con il gruppo davanti al testo e il mittente
  push(meshFrame("IU5XXX-12", "22251", "Ci sono, 59 qui", id++));
  CHECK(s.b.note.received.size() == 1 && strcmp(s.b.node.inbox().text(), "[22251] Ci sono, 59 qui") == 0);
  CHECK(strcmp(s.b.node.inbox().callsign(), "IU5XXX-12") == 0 && logHas("MOSTRATO"));
  // altro gruppo impostato: si vede; un gruppo non impostato, o "tutti", no
  push(meshFrame("IU5YYY-9", "222", "Ciao Italia", id++));
  CHECK(s.b.note.received.size() == 2 && strcmp(s.b.node.inbox().text(), "[222] Ciao Italia") == 0);
  push(meshFrame("IU5YYY-9", "22299", "altro gruppo", id++));
  push(meshFrame("IU5YYY-9", "*", "a tutti", id++));
  push(meshFrame("IU5YYY-9", "IU5ABC-9", "ad un altro", id++));  // un nominativo impostato non e' un gruppo: non e' per noi
  CHECK(s.b.note.received.size() == 2 && logHas("non e' per noi"));
  // la richiesta di conferma in un gruppo non e' un obbligo: il testo si mostra senza "{NNN" e nessuna conferma parte
  s.air.clock = &s.clock;
  push(meshFrame("IU5XXX-12", "22251", "Ricevuto{77", id++));
  s.run(3000);
  CHECK(strcmp(s.b.node.inbox().text(), "[22251] Ricevuto") == 0 && s.air.sent[1] == 0);
  // messaggio diretto a noi con il solo nominativo (senza SSID): si accetta, e la conferma torna a chi ha scritto
  push(meshFrame("IU5XXX-12", "IZ5ABC", "Privato senza SSID{5", id++));
  s.run(3000);
  CHECK(strcmp(s.b.node.inbox().text(), "Privato senza SSID") == 0 && s.air.sent[1] == 1);
  // un testo di gruppo molto lungo: tagliato a 150 compreso il gruppo
  std::string longText(190, 'a');
  push(meshFrame("IU5XXX-12", "22251", longText.c_str(), id++));
  CHECK(s.b.node.inbox().textLen() == kMaxTextChars && strncmp(s.b.node.inbox().text(), "[22251] aaa", 11) == 0);
  // diagnostica: FCS sbagliata e frame non valido compaiono nel registro
  g_meshLog.clear();
  auto bad = meshFrame("IU5XXX-12", "22251", "FCS rotta", id++);
  bad[12] ^= 0x01;
  push(bad);
  CHECK(logHas("FCS sbagliata"));
  push(std::vector<uint8_t>(30, 0x55));
  CHECK(logHas("frame non valido"));
  push(meshFrame("OE1XAR-22", "*", "{CET}2026-11-11 15:32:42", id++));
  CHECK(logHas("servizio"));
  push(meshFrame("IU5YYY-9", "IZ5ABC-99", "IZ5ABC-99:ack1", id++));
  CHECK(logHas("conferma di un nostro testo"));
  // riepilogo a fine escursione: tutti i frame ricevuti divisi per tipo
  g_meshLog.clear();
  s.b.node.endMeshcom();
  CHECK(logHas("fine escursione: 12 frame ricevuti (testi 10, posizioni e rapporti 0, FCS sbagliata 1, non validi 1)"));
  // senza gruppi impostati nessun gruppo e' mostrato
  Sim t;
  t.run(100);
  CHECK(t.b.node.startMeshcom(nullptr, nullptr, t.clock, kWin));
  t.air.inbox[1].push_back(meshFrame("IU5XXX-12", "22251", "niente gruppi", 0x500));
  t.run(300);
  CHECK(t.b.note.received.empty());
}

int main() {
  testExpectedPackets();
  testMeshcomSendReceive();
  testMeshcomWindow();
  testMeshcomRefused();
  testMeshcomFilters();
  testMeshcomAck();
  testMeshcomGroups();
  testTextMessage();
  testTextRules();
  testVoiceMessageStopAndReplace();
  testMessageProgress();
  testProgressLostTail();
  testNoProgressInStandard();
  testPositionOffByDefault();
  testPositionInStartOnly();
  testPositionNotSentWithoutFix();
  testMessageProfile();
  testQualityProfile();
  testMessageEndLost();
  testMessageProfileRules();
  testRecordThenSend();
  testRecordTurnLimit();
  testRecordModeOffByDefault();
  testPacketSpacing();
  testTripleClickTogglesTestMode();
  testSlowClicksDoNothing();
  testShortTapDoesNotTransmit();
  testClicksWorkWithoutCallsign();
  testLongPressAfterClicks();
  testOneTurn();
  testReplyAfterWhite();
  testPttDenied();
  testTurnLimit();
  testPlaceholderCallsignBlocksTx();
  testLossOnAir();
  TEST_MAIN_END();
}
