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

static void testOneTurn() {
  Sim s;
  s.run(100);
  s.a.node.pttPressed(s.clock);
  CHECK(s.a.node.state() == VoiceNode::State::Transmitting);
  s.run(3000);
  s.a.node.pttReleased(s.clock);
  s.run(6000);

  const int n = static_cast<int>(s.a.mic.produced());
  CHECK(n >= 70 && n <= 76);  // ~3 s di voce
  CHECK(s.b.spk.played == seq(n));
  CHECK(s.b.note.callsign == "IU5HJU");
  CHECK(s.b.note.rxStarts == 1 && s.b.note.rxEnds == 1);
  CHECK(s.a.note.txStarts == 1 && s.a.note.txEnds == 1 && !s.a.note.limitHit);
  CHECK(s.a.node.state() == VoiceNode::State::Idle);
  CHECK(s.b.node.state() == VoiceNode::State::Idle);
  CHECK(s.a.spk.played.empty());  // chi trasmette non riproduce
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
  CHECK(silent.node.state() == VoiceNode::State::Idle);
  CHECK(silent.note.denied == 1 && silent.note.lastDeny == DenyReason::NoCallsign);
  CHECK(silent.note.txStarts == 0 && air.sent[0] == 0);
  Station other(air, 0, "NOCALL", clock);
  other.node.pttPressed(clock);
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

int main() {
  testOneTurn();
  testReplyAfterWhite();
  testPttDenied();
  testTurnLimit();
  testPlaceholderCallsignBlocksTx();
  testLossOnAir();
  TEST_MAIN_END();
}
