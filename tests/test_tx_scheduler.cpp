// Regista della scheda TX del ripetitore: voce prioritaria, ascolto messaggi, ritrasmissione, attese e interruzioni.
#include <cstring>
#include <memory>
#include <vector>

#include "fakes.h"
#include "packet.h"
#include "tx_scheduler.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;

static const Profile& V = profileFor(kProfileStandard);
static const Profile& M = profileFor(kProfileMessage);

static std::vector<Packet> makeMessage(uint16_t session, int frames) {
  std::vector<Packet> v;
  TxSession tx("IU5HJU", session, true);
  tx.setExpectedTotal(TxSession::expectedPackets(frames, true));
  auto drain = [&]() {
    Packet p;
    while (tx.popPacket(p)) v.push_back(p);
  };
  tx.begin();
  drain();
  for (int i = 0; i < frames; i++) {
    uint8_t f[kFrameBytes];
    for (size_t k = 0; k < kFrameBytes; k++) f[k] = static_cast<uint8_t>(i * 7 + k * 31 + 1);
    tx.pushFrame(f);
    drain();
  }
  tx.finish();
  drain();
  return v;
}

static std::vector<uint8_t> wireOf(const Packet& p) {
  uint8_t b[kMaxWireBytes];
  const size_t n = encodePacket(p, b, sizeof(b));
  return std::vector<uint8_t>(b, b + n);
}

struct Rig {
  fake::Air air;
  fake::Radio radio{air, 0};
  std::unique_ptr<MessageStore> store{new MessageStore};
  std::unique_ptr<TxScheduler> sch{new TxScheduler(radio, *store, V, M, 433500, 0x4000)};
  uint32_t t = 0;

  // Il messaggio arriva sull'uscita e il regista lo riceve (in ascolto); t avanza come in aria.
  void deliver(const std::vector<Packet>& msg) {
    for (const Packet& p : msg) {
      air.inbox[0].push_back(wireOf(p));
      sch->tick(t, true);
      t += 200;
    }
  }
  void run(uint32_t ms, uint32_t step = 100) {
    const uint32_t end = t + ms;
    while (t < end) {
      sch->tick(t, true);
      t += step;
    }
  }
  void voice(int n, uint32_t gap = 100) {
    Packet p;
    p.mode = kModeCodec2_1200;
    p.session = 0x0AAA;
    p.payloadLen = 6 * 12;
    for (int i = 0; i < n; i++) {
      p.seq = static_cast<uint8_t>(i);
      sch->onLinkPacket(wireOf(p).data(), wireOf(p).size());
      sch->tick(t, true);
      t += gap;
    }
  }
};

static void testStartsOnVoiceThenListens() {
  Rig r;
  CHECK(r.sch->state() == TxScheduler::State::Voice && r.radio.params.empty());
  r.sch->tick(0, true);
  CHECK(r.sch->state() == TxScheduler::State::Listen);
  CHECK(r.radio.params.size() == 1 && r.radio.params[0].id == kProfileMessage && r.radio.params[0].khz == 433500);
}

static void testVoiceUsesVoiceParams() {
  Rig r;
  r.run(500);
  r.voice(5);
  CHECK(r.sch->state() == TxScheduler::State::Voice);
  CHECK(r.radio.curId == kProfileStandard);
  CHECK(r.air.sent[0] == 5 && r.sch->stats().voiceSent == 5);
  r.run(TxScheduler::kHoldMs - 500);  // ancora nel mantenimento
  CHECK(r.sch->state() == TxScheduler::State::Voice);
  r.run(1500);
  CHECK(r.sch->state() == TxScheduler::State::Listen && r.radio.curId == kProfileMessage);
}

static void testMessageStoredAndRelayedAfterTail() {
  Rig r;
  r.run(500);
  const auto msg = makeMessage(0x1111, 100);
  r.deliver(msg);
  CHECK(r.store->readyCount() == 1);
  CHECK(r.air.sent[0] == 0);  // in ascolto non si trasmette: la ritrasmissione e' dopo
  r.run(TxScheduler::kBetweenMsgMs - 1000);
  CHECK(r.air.sent[0] == 0);  // dopo l'ultimo pacchetto ricevuto si lascia tempo a chi trasmette
  r.run(4000);
  CHECK(r.sch->stats().relayed == 1);
  CHECK(r.air.sent[0] == static_cast<int>(msg.size()));
  CHECK(r.store->readyCount() == 0);
  // sessione nuova uguale per tutti i pacchetti, flag RELAYED, stesso contenuto
  Packet first, last;
  CHECK(decodePacket(r.air.log[0][0].data(), r.air.log[0][0].size(), first));
  CHECK(decodePacket(r.air.log[0].back().data(), r.air.log[0].back().size(), last));
  CHECK(first.session == 0x4000 && last.session == 0x4000 && (first.flags & kFlagRelayed) && (first.flags & kFlagStart));
  CHECK(strcmp(first.callsign, "IU5HJU") == 0);
  bool hasEnd = false;
  for (const auto& w : r.air.log[0]) {
    Packet q;
    if (decodePacket(w.data(), w.size(), q) && (q.flags & kFlagEnd)) hasEnd = true;
  }
  CHECK(hasEnd);
  CHECK(r.radio.curId == kProfileMessage);  // niente cambi di parametri inutili: solo il primo
  CHECK(r.radio.params.size() == 1);
}

static void testWaitsTailAfterVoice() {
  Rig r;
  r.run(500);
  const auto msg = makeMessage(0x1111, 40);
  r.deliver(msg);
  const uint32_t before = r.air.sent[0];
  CHECK(before == 0);
  // la voce passa subito dopo il messaggio: il messaggio non parte per kTailMs dalla fine della voce
  r.voice(5);
  const uint32_t voiceEnd = r.t;
  r.run(TxScheduler::kTailMs - 3000);
  CHECK(r.sch->stats().relayPackets == 0);
  r.run(2000 + 3000 + TxScheduler::kBetweenMsgMs);
  CHECK(r.sch->stats().relayed == 1);
  CHECK(r.sch->stats().relayPackets == msg.size());
  (void)voiceEnd;
}

static void testVoiceInterruptsAndMessageRestarts() {
  Rig r;
  r.run(500);
  const auto msg = makeMessage(0x1111, 100);
  r.deliver(msg);
  while (r.sch->stats().relayPackets < 3) r.run(100);
  CHECK(r.sch->state() == TxScheduler::State::Relay);
  const uint32_t half = r.sch->stats().relayPackets;
  CHECK(half > 0 && half < msg.size());
  r.voice(4);  // arriva voce: il messaggio si ferma
  CHECK(r.sch->stats().interrupted == 1 && r.sch->state() == TxScheduler::State::Voice);
  CHECK(r.radio.curId == kProfileStandard);
  CHECK(r.store->readyCount() == 1);  // resta nell'archivio
  r.run(TxScheduler::kTailMs + TxScheduler::kBetweenMsgMs + 8000);
  CHECK(r.sch->stats().relayed == 1);
  CHECK(r.sch->stats().relayPackets == half + msg.size());  // ripartito dall'inizio
  // il secondo tentativo ha una sessione nuova e comincia con lo START
  Packet p;
  CHECK(decodePacket(r.air.log[0][r.air.log[0].size() - msg.size()].data(), r.air.log[0][r.air.log[0].size() - msg.size()].size(), p));
  CHECK((p.flags & kFlagStart) && p.session == 0x4001);
}

static void testBusyChannel() {
  Rig r;
  r.run(500);
  const auto msg = makeMessage(0x1111, 40);
  r.deliver(msg);
  r.air.busy = true;
  r.run(15000);
  CHECK(r.sch->stats().relayPackets == 0 && r.store->readyCount() == 1);
  r.air.busy = false;
  while (r.sch->stats().relayPackets < 2) r.run(100);
  CHECK(r.sch->state() == TxScheduler::State::Relay);
  // il canale si occupa a meta': dopo kBusyAbortMs il messaggio si interrompe e ripartira' dall'inizio
  r.air.busy = true;
  r.run(TxScheduler::kBusyAbortMs + 1000);
  CHECK(r.sch->stats().busyAborts == 1 && r.sch->state() == TxScheduler::State::Listen);
  r.air.busy = false;
  const uint32_t sent = r.sch->stats().relayPackets;
  r.run(TxScheduler::kBetweenMsgMs + 6000);
  CHECK(r.sch->stats().relayed == 1 && r.sch->stats().relayPackets > sent + 3);
}

static void testNotReceivingWhileMessageIncoming() {
  Rig r;
  r.run(500);
  const auto msg = makeMessage(0x1111, 100);
  // a meta' ricezione: non si trasmette un altro messaggio (non ce n'e') ne' si comincia la ritrasmissione
  std::vector<Packet> halfMsg(msg.begin(), msg.begin() + msg.size() / 2);
  r.deliver(halfMsg);
  r.run(2000);
  CHECK(r.store->receiving() && r.sch->stats().relayPackets == 0);
  // silenzio oltre il limite: il messaggio incompleto si abbandona
  r.run(MessageStore::kSilenceMs + 1000);
  CHECK(!r.store->receiving() && r.store->readyCount() == 0 && r.store->stats().abandoned == 1);
}

static void testMessagesQueueInOrder() {
  Rig r;
  r.run(500);
  const auto a = makeMessage(0x1111, 40), b = makeMessage(0x2222, 40);
  r.deliver(a);
  r.deliver(b);
  CHECK(r.store->readyCount() == 2);
  r.run(30000);
  CHECK(r.sch->stats().relayed == 2 && r.store->readyCount() == 0);
  Packet p;
  CHECK(decodePacket(r.air.log[0][a.size()].data(), r.air.log[0][a.size()].size(), p));
  CHECK((p.flags & kFlagStart) && p.session == 0x4001);  // il secondo ha un'altra sessione
}

static void testNoCallsignDoesNotTransmit() {
  Rig r;
  r.run(500);
  r.deliver(makeMessage(0x1111, 40));
  r.voice(0);
  Packet p;
  p.mode = kModeCodec2_1200;
  p.payloadLen = 6 * 12;
  r.sch->onLinkPacket(wireOf(p).data(), wireOf(p).size());
  for (int i = 0; i < 300; i++) {
    r.sch->tick(r.t, false);
    r.t += 100;
  }
  CHECK(r.air.sent[0] == 0 && r.sch->queue().size() == 0);  // la voce in coda si scarta, il messaggio aspetta
  CHECK(r.sch->stats().voiceSent == 0 && r.store->readyCount() == 1);
}

static void testParamFailureRetries() {
  Rig r;
  r.radio.failParams = true;
  r.run(2500);
  CHECK(r.sch->state() == TxScheduler::State::Voice && r.sch->stats().paramErrors >= 2 && r.sch->stats().paramErrors <= 3);
  r.radio.failParams = false;
  r.run(1500);
  CHECK(r.sch->state() == TxScheduler::State::Listen);
}

static void testRelayedPacketsIgnoredWhenListening() {
  Rig r;
  r.run(500);
  auto msg = makeMessage(0x1111, 40);
  for (Packet& p : msg) p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);  // gia' ripetuto da un altro ripetitore
  r.deliver(msg);
  CHECK(r.store->readyCount() == 0);
}

static void testMessagesDisabled() {
  Rig r;
  r.sch->setMessagesEnabled(false);
  r.run(3000);
  CHECK(r.sch->state() == TxScheduler::State::Voice && r.radio.params.empty());
  r.voice(3);
  CHECK(r.air.sent[0] == 3);
  r.run(6000);
  CHECK(r.sch->state() == TxScheduler::State::Voice && r.radio.params.empty());
}

static Packet dec(const std::vector<uint8_t>& w) {
  Packet p;
  CHECK(decodePacket(w.data(), w.size(), p));
  return p;
}

static void testAnnounceBeforeAndAfter() {
  Rig r;
  r.sch->setCallsign("IZ0RPT");
  r.run(500);
  const auto msg = makeMessage(0x1111, 100);
  r.deliver(msg);
  const uint32_t expect = (r.store->readyAirtimeMs(M) + static_cast<uint32_t>(msg.size()) * kInterPacketGapMs + 2000 + 999) / 1000;
  r.run(15000);
  CHECK(r.sch->stats().relayed == 1 && r.sch->state() == TxScheduler::State::Listen);
  const auto& log = r.air.log[0];
  CHECK(log.size() == msg.size() + 4);
  const Packet a1 = dec(log[0]), a2 = dec(log[1]);
  CHECK(a1.mode == kModeBusy && a2.mode == kModeBusy && a1.session == a2.session && strcmp(a1.callsign, "IZ0RPT") == 0);
  CHECK(busySeconds(a1) == expect && expect > 5);
  CHECK(dec(log[2]).mode == kModeCodec2_1200 && (dec(log[2]).flags & kFlagRelayed));
  const Packet f1 = dec(log[log.size() - 2]), f2 = dec(log[log.size() - 1]);
  CHECK(f1.mode == kModeBusy && f2.mode == kModeBusy && busySeconds(f1) == 0 && f1.session == f2.session && f1.session != a1.session);
  // parametri: ascolto, voce (annuncio), messaggio (ritrasmissione), voce (fine), messaggio (ascolto)
  CHECK(r.radio.params.size() == 5);
  const uint8_t ids[5] = {kProfileMessage, kProfileStandard, kProfileMessage, kProfileStandard, kProfileMessage};
  for (size_t i = 0; i < r.radio.params.size() && i < 5; i++) CHECK(r.radio.params[i].id == ids[i]);
}

static void testVoiceInterruptsAnnounce() {
  Rig r;
  r.sch->setCallsign("IZ0RPT");
  r.run(500);
  r.deliver(makeMessage(0x1111, 100));
  while (r.sch->stats().announces < 1) r.run(100);
  CHECK(r.sch->state() == TxScheduler::State::Announce);
  r.voice(3);
  CHECK(r.sch->state() == TxScheduler::State::Voice && r.sch->stats().relayPackets == 0 && r.store->readyCount() == 1);
  r.run(TxScheduler::kTailMs + 20000);
  CHECK(r.sch->stats().relayed == 1);  // alla fine riparte con un annuncio nuovo
  CHECK(r.sch->stats().announces >= 5);
}

static void testBusyAbortReleases() {
  Rig r;
  r.sch->setCallsign("IZ0RPT");
  r.run(500);
  r.deliver(makeMessage(0x1111, 100));
  while (r.sch->stats().relayPackets < 2) r.run(100);
  r.air.busy = true;
  r.run(TxScheduler::kBusyAbortMs + 3000);
  CHECK(r.sch->stats().busyAborts == 1);
  const auto& log = r.air.log[0];
  const Packet last = dec(log.back());
  CHECK(last.mode == kModeBusy && busySeconds(last) == 0);
}

static void testVoiceInterruptsRelayNoRelease() {
  Rig r;
  r.sch->setCallsign("IZ0RPT");
  r.run(500);
  r.deliver(makeMessage(0x1111, 100));
  while (r.sch->stats().relayPackets < 3) r.run(100);
  const int before = r.sch->stats().announces;
  r.voice(2);
  CHECK(r.sch->stats().interrupted == 1 && static_cast<int>(r.sch->stats().announces) == before);  // niente annuncio di fine: basta la voce
}

int main() {
  testAnnounceBeforeAndAfter();
  testVoiceInterruptsAnnounce();
  testBusyAbortReleases();
  testVoiceInterruptsRelayNoRelease();
  testMessagesDisabled();
  testStartsOnVoiceThenListens();
  testVoiceUsesVoiceParams();
  testMessageStoredAndRelayedAfterTail();
  testWaitsTailAfterVoice();
  testVoiceInterruptsAndMessageRestarts();
  testBusyChannel();
  testNotReceivingWhileMessageIncoming();
  testMessagesQueueInOrder();
  testNoCallsignDoesNotTransmit();
  testParamFailureRetries();
  testRelayedPacketsIgnoredWhenListening();
  TEST_MAIN_END();
}
