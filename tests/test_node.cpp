// Test end-to-end della macchina a stati con due nodi su un canale simulato.
#include <memory>

#include "fakes.h"
#include "testing.h"
#include "voice_node.h"

using namespace lcr;

static uint16_t nextSession() {
  static uint16_t s = 100;
  return s++;
}

struct Station {
  Station(fake::Air& air, int id, const char* cs, const uint32_t& clock, uint8_t profile = kProfileStandard, uint32_t extraGapMs = 0)
      : radio(air, id), mic(clock), node(makeCfg(cs, profile, extraGapMs), radio, codec, mic, spk, note) {}
  static NodeConfig makeCfg(const char* cs, uint8_t profile = kProfileStandard, uint32_t extraGapMs = 0) {
    NodeConfig c;
    strcpy(c.callsign, cs);
    c.newSessionId = nextSession;
    c.profile = profile;
    c.extraGapMs = extraGapMs;
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
  CHECK(s.b.spk.played == seq(n));  // tutto il messaggio, in ordine, dopo la fine della ricezione
  CHECK(s.b.note.playbackStarts == 1 && s.b.note.rxStarts == 1 && s.b.note.rxEnds == 1);
  CHECK(s.b.note.callsign == "IU5HJU");
  CHECK(s.a.node.state() == VoiceNode::State::Idle && s.b.node.state() == VoiceNode::State::Idle);
  CHECK(s.a.note.txEnds == 1 && !s.a.note.limitHit);
  // Spenta o accesa che sia, la registrazione resta attiva nel profilo Messaggio
  s.a.node.setRecordMode(false);
  CHECK(s.a.node.recordMode());
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
  CHECK(s.b.note.playbackStarts == 1);
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
  // Un messaggio molto lungo si ferma al limite del profilo (10 s di voce)
  s.a.node.pttPressed(s.clock);
  s.run(20000);
  CHECK(s.a.node.recPhase() != VoiceNode::RecPhase::Recording);  // la registrazione si e' fermata da sola a 10 s
  s.a.node.pttReleased(s.clock);
  s.run(150000);
  CHECK(s.a.note.limitHit);  // e il limite e' stato segnalato
  CHECK(static_cast<uint32_t>(s.a.mic.produced()) == profileFor(kProfileMessage).maxTurnMs / kFrameMs);
  CHECK(s.b.spk.played == seq(static_cast<int>(profileFor(kProfileMessage).maxTurnMs / kFrameMs)));
}

int main() {
  testMessageProfile();
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
