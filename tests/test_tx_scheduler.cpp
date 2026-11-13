// Regista della scheda TX del ripetitore: voce prioritaria, ascolto messaggi, ritrasmissione, attese e interruzioni.
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "fakes.h"
#include "packet.h"
#include "tx_scheduler.h"
#include "testing.h"
#include "radio_test.h"
#include "sysop.h"
#include "tx_session.h"

using namespace lcr;

static const Profile& V = profileFor(kProfileRange);
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
  CHECK(r.radio.curId == kProfileRange);
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
  CHECK(r.radio.curId == kProfileRange);
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
  const uint8_t ids[5] = {kProfileMessage, kProfileRange, kProfileMessage, kProfileRange, kProfileMessage};
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

static const uint8_t KEY[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

struct CtlRig : Rig {
  RepeaterControl ctl{2, 17, 10};
  CtlRig() {
    sch->setCallsign("IZ0RPT");
    sch->setControl(&ctl, KEY);
  }
  RepeaterControl::Effects fx;
};

static void testControlReplyAndOff() {
  CtlRig r;
  r.run(500);
  CHECK(r.sch->onControl({kSysopStatus, 0, 1}, r.t, r.fx) == kSysopOk);
  CHECK(r.air.sent[0] == 1 && r.radio.curId == kProfileRange);  // la risposta esce con i parametri della voce
  SysopReply rep;
  Packet p = dec(r.air.log[0][0]);
  CHECK(p.mode == kModeSysop && parseSysopReply(p, KEY, rep) && rep.code == kSysopStatus && rep.counter == 1 && rep.result == kSysopOk);
  CHECK((rep.data >> 16 & 0xFF) == 10 && (rep.data >> 24) == 0);
  // SPEGNI: la risposta esce (ultima trasmissione), poi niente passa piu', ne' la voce ne' i messaggi
  CHECK(r.sch->onControl({kSysopOff, 0, 2}, r.t, r.fx) == kSysopOk && r.fx.forceReply);
  CHECK(r.air.sent[0] == 2 && parseSysopReply(dec(r.air.log[0][1]), KEY, rep) && rep.code == kSysopOff && (rep.data >> 24) == 1);
  r.voice(5);
  CHECK(r.air.sent[0] == 2 && r.sch->stats().voiceSent == 0);
  r.run(5000);
  // STATO a TX spenta: la risposta non si trasmette (non si trasmette nulla)
  CHECK(r.sch->onControl({kSysopStatus, 0, 3}, r.t, r.fx) == kSysopOk && r.air.sent[0] == 2 && r.sch->stats().repliesDropped == 1);
  // RIACCENDI: risponde e la voce torna a passare
  CHECK(r.sch->onControl({kSysopOn, 0, 4}, r.t, r.fx) == kSysopOk && r.air.sent[0] == 3);
  CHECK(parseSysopReply(dec(r.air.log[0][2]), KEY, rep) && rep.code == kSysopOn && (rep.data >> 24) == 0);
  r.voice(3);
  CHECK(r.sch->stats().voiceSent == 3);
}

static void testControlOffMinutes() {
  CtlRig r;
  r.run(500);
  r.sch->onControl({kSysopOffMinutes, 1, 1}, r.t, r.fx);
  const int sent = r.air.sent[0];
  r.voice(2);
  CHECK(r.air.sent[0] == sent);
  r.run(61000);
  r.voice(2);
  CHECK(r.air.sent[0] == sent + 2);  // scaduto: torna in servizio da solo
}

static void testControlStopsRelay() {
  CtlRig r;
  r.run(500);
  r.deliver(makeMessage(0x1111, 100));
  while (r.sch->stats().relayPackets < 3) r.run(100);
  const uint32_t before = r.sch->stats().relayPackets;
  r.sch->onControl({kSysopOff, 0, 1}, r.t, r.fx);  // spegne a meta' messaggio
  r.run(30000);
  CHECK(r.sch->stats().relayPackets == before && r.store->readyCount() == 1);
  r.sch->onControl({kSysopOn, 0, 2}, r.t, r.fx);
  r.run(TxScheduler::kBetweenMsgMs + 30000);
  CHECK(r.sch->stats().relayed == 1);  // riparte e arriva in fondo
}

static void testControlIdentifyAndPower() {
  CtlRig r;
  r.run(500);
  CHECK(r.sch->onControl({kSysopIdentify, 0, 1}, r.t, r.fx) == kSysopOk && r.fx.identify);
  CHECK(r.air.sent[0] == 2);  // risposta e identificazione
  Packet id = dec(r.air.log[0][1]);
  CHECK(id.mode == kModeText && strcmp(id.callsign, "IZ0RPT") == 0 && (id.flags & kFlagStart));
  CHECK(r.sch->onControl({kSysopPower, 12, 2}, r.t, r.fx) == kSysopOk && r.fx.power == 12);
  CHECK(r.radio.powers.size() == 1 && r.radio.powers[0] == 12);
  CHECK(r.sch->onControl({kSysopPower, 30, 3}, r.t, r.fx) == kSysopBadParam && r.radio.powers.size() == 1);
  SysopReply rep;
  CHECK(parseSysopReply(dec(r.air.log[0].back()), KEY, rep) && rep.result == kSysopBadParam);
  CHECK(r.sch->onControl({77, 0, 4}, r.t, r.fx) == kSysopUnknown);
  // senza regista di controllo i comandi non si eseguono
  Rig plain;
  CHECK(plain.sch->onControl({kSysopStatus, 0, 1}, 0, r.fx) == kSysopNotAllowed && plain.air.sent[0] == 0);
}

static void testSafetyTripsOnLongVoice() {
  CtlRig r;
  r.run(500);
  // una voce che non finisce mai (pacchetti con pausa minima): dopo il limite la TX si spegne da sola
  Packet p;
  p.mode = kModeCodec2_1200;
  p.session = 0x0AAA;
  p.payloadLen = 6 * 12;
  int n = 0;
  while (r.ctl.txAllowed(r.t) && n < 5000) {
    p.seq = static_cast<uint8_t>(n % 100);
    r.sch->onLinkPacket(wireOf(p).data(), wireOf(p).size());
    r.sch->tick(r.t, true);
    r.t += 200;
    n++;
  }
  CHECK(r.ctl.safetyTrips() == 1 && n < 5000);
  const int sent = r.air.sent[0];
  r.sch->onLinkPacket(wireOf(p).data(), wireOf(p).size());
  r.run(2000);
  CHECK(r.air.sent[0] == sent);
}

static void testStaleReply() {
  CtlRig r;
  r.run(500);
  r.sch->onStaleCommand(kSysopOff, 41, r.t);
  CHECK(r.air.sent[0] == 1);
  SysopReply rep;
  CHECK(parseSysopReply(dec(r.air.log[0][0]), KEY, rep) && rep.result == kSysopCounterLow && rep.counter == 41 && rep.code == kSysopOff);
  r.run(2000);
  r.sch->onStaleCommand(kSysopOff, 41, r.t);  // troppo presto: niente
  CHECK(r.air.sent[0] == 1);
  r.run(TxScheduler::kStaleReplyGapMs);
  r.sch->onStaleCommand(kSysopOff, 41, r.t);
  CHECK(r.air.sent[0] == 2);
  r.sch->onControl({kSysopOff, 0, 50}, r.t, r.fx);  // TX spenta: niente risposte di risincronizzazione
  const int sent = r.air.sent[0];
  r.run(TxScheduler::kStaleReplyGapMs + 1000);
  r.sch->onStaleCommand(kSysopOff, 50, r.t);
  CHECK(r.air.sent[0] == sent);
}

static int countBeacons(const Rig& r) {
  int n = 0;
  for (const auto& w : r.air.log[0]) {
    Packet p;
    if (decodePacket(w.data(), w.size(), p) && p.mode == kModeText && p.seq == kTextSeqBeacon) {
      n++;
      CHECK(strcmp(p.callsign, "IZ0RPT") == 0 && (p.flags & kFlagStart) && (p.flags & kFlagEnd) && strncmp(reinterpret_cast<const char*>(p.payload), "Ripetitore VoLoRa", p.payloadLen) == 0);
    }
  }
  return n;
}

static void testBeaconPeriodic() {
  Rig r;
  r.sch->setCallsign("IZ0RPT");
  r.sch->setBeaconInterval(60000);
  r.run(14000);
  CHECK(countBeacons(r) == 0);          // il primo dopo 15 s dall'avvio
  r.run(2000);
  CHECK(countBeacons(r) == 1 && r.sch->stats().beacons == 1);
  r.run(58000);
  CHECK(countBeacons(r) == 1);
  r.run(3000);
  CHECK(countBeacons(r) == 2);
  r.run(180000);
  CHECK(countBeacons(r) == 5);          // uno al minuto
  // sessioni diverse: le stazioni non scartano il secondo come copia
  Packet a, b;
  int found = 0;
  for (const auto& w : r.air.log[0]) {
    Packet p;
    if (decodePacket(w.data(), w.size(), p) && p.seq == kTextSeqBeacon) (found++ == 0 ? a : b) = p;
  }
  CHECK(found >= 2 && a.session != b.session);
  // la radio e' tornata in ascolto messaggi dopo l'identificativo (parametri della voce per 3 s)
  CHECK(r.radio.curId == kProfileMessage);
}

static void testBeaconOff() {
  Rig r;  // senza nominativo o senza intervallo non parte
  r.sch->setBeaconInterval(60000);
  r.run(120000);
  CHECK(r.air.sent[0] == 0);
  Rig q;
  q.sch->setCallsign("IZ0RPT");
  q.run(120000);
  CHECK(q.air.sent[0] == 0);
  Rig z;  // N0CALL (mayTransmit false) o TX spenta: niente
  z.sch->setCallsign("IZ0RPT");
  z.sch->setBeaconInterval(60000);
  for (int i = 0; i < 1500; i++) {
    z.sch->tick(z.t, false);
    z.t += 100;
  }
  CHECK(z.air.sent[0] == 0);
}

static void testBeaconWaitsForVoiceAndMessages() {
  CtlRig r;
  r.sch->setBeaconInterval(60000);
  r.run(14500);
  // arriva voce proprio quando l'identificativo e' dovuto: prima la voce, l'identificativo dopo
  r.voice(40, 100);
  CHECK(countBeacons(r) == 0);
  r.run(500);
  CHECK(countBeacons(r) == 1);  // finita la voce, subito dopo parte l'identificativo (era dovuto)
  r.run(4000);
  CHECK(countBeacons(r) == 1);
  // un messaggio in ritrasmissione non e' interrotto: l'identificativo aspetta la fine
  r.run(30000);
  r.deliver(makeMessage(0x1111, 100));
  while (r.sch->stats().relayPackets < 3) r.run(100);
  CHECK(countBeacons(r) == 1);
  r.run(40000);  // la scadenza dei 60 s passa a meta' o a fine messaggio: comunque il messaggio finisce per intero
  CHECK(r.sch->stats().relayed == 1 && r.sch->stats().interrupted == 0);
  r.run(3000);
  CHECK(countBeacons(r) == 2);
  // TX spenta dal sysop: nessun identificativo
  r.sch->onControl({kSysopOff, 0, 1}, r.t, r.fx);
  const int n = countBeacons(r);
  r.run(200000);
  CHECK(countBeacons(r) == n);
  r.sch->onControl({kSysopOn, 0, 2}, r.t, r.fx);
  r.run(2000);
  CHECK(countBeacons(r) == n + 1);
}

static void testMorseId() {
  Rig r;
  r.sch->setCallsign("IZ0RPT");
  r.sch->setBeaconInterval(60000);
  r.sch->setMorseId(true, 15, 700.0f, 2.5f);
  r.run(14000);
  CHECK(r.radio.fskBegins == 0);
  r.run(1500);  // il testo esce a 15 s, poi parte il Morse
  CHECK(countBeacons(r) == 1 && r.sch->state() == TxScheduler::State::Morse && r.radio.fskBegins == 1 && r.radio.inFsk);
  CHECK(std::fabs(r.radio.fskMhz - 433.5f) < 1e-3 && std::fabs(r.radio.fskKbps - 1.4f) < 1e-4 && std::fabs(r.radio.fskDev - 2.5f) < 1e-4);
  r.run(12000);
  CHECK(r.sch->state() != TxScheduler::State::Morse && !r.radio.inFsk && r.radio.fskEnds == 1);
  CHECK(r.sch->stats().morseIds == 1 && r.sch->stats().morseAborted == 0);
  // i pacchetti sono quelli del piano: tono a 0xAA, durate e byte come planMorseBursts
  RadioTestRequest rq;
  rq.text[0] = 0;
  strcpy(rq.text, "IZ0RPT");
  rq.wpm = 15;
  rq.toneHz = 700;
  MorseBurst plan[160];
  size_t tones = 0;
  const size_t n = planMorseBursts(rq, plan, 160, nullptr);
  // la portante non si spegne mai: prima del primo tono e nelle pause ci sono pacchetti 0xFF, i toni sono 0xAA nell'ordine del piano
  std::vector<fake::Radio::FskBurst> toneBursts;
  size_t carriers = 0;
  for (const auto& fb : r.radio.fskBursts) {
    if (fb.pattern == 0xAA) toneBursts.push_back(fb);
    else if (fb.pattern == 0xFF) carriers++;
  }
  CHECK(!r.radio.fskBursts.empty() && r.radio.fskBursts[0].pattern == 0xFF && r.radio.fskBursts.back().pattern == 0xFF);
  CHECK(carriers + toneBursts.size() == r.radio.fskBursts.size() && carriers > 5);
  for (size_t i = 0; i < n; i++) {
    if (!plan[i].tone) continue;
    CHECK(tones < toneBursts.size() && toneBursts[tones].bytes == plan[i].bytes);
    tones++;
  }
  CHECK(tones == toneBursts.size() && tones > 10);
  // dopo il Morse si torna ad ascoltare i messaggi (parametri del Messaggio)
  r.run(5000);
  CHECK(r.sch->state() == TxScheduler::State::Listen && r.radio.curId == kProfileMessage);
  // il secondo identificativo, un minuto dopo il primo
  r.run(60000);
  CHECK(r.radio.fskBegins == 2 && countBeacons(r) == 2);
}

static void testMorseOffByDefault() {
  Rig r;
  r.sch->setCallsign("IZ0RPT");
  r.sch->setBeaconInterval(60000);
  r.run(60000);
  CHECK(countBeacons(r) >= 1 && r.radio.fskBegins == 0 && r.sch->stats().morseIds == 0);
  Rig q;
  q.sch->setCallsign("IZ0RPT");
  q.sch->setBeaconInterval(60000);
  q.sch->setMorseId(false);
  q.run(60000);
  CHECK(q.radio.fskBegins == 0);
}

static void testMorseAbortedByVoice() {
  Rig r;
  r.sch->setCallsign("IZ0RPT");
  r.sch->setBeaconInterval(60000);
  r.sch->setMorseId(true);
  r.run(18000);
  CHECK(r.sch->state() == TxScheduler::State::Morse);
  r.voice(3);  // la voce passa: il Morse si ferma, la radio torna LoRa con i parametri della voce
  CHECK(r.sch->state() == TxScheduler::State::Voice && !r.radio.inFsk && r.radio.fskEnds == 1 && r.sch->stats().morseAborted == 1);
  CHECK(r.sch->stats().voiceSent == 3);
  r.run(10000);
  CHECK(r.sch->stats().morseIds == 0 && r.radio.fskBegins == 1);  // non riparte da solo: il prossimo e' al prossimo periodo
}

static void testMorseFailures() {
  Rig r;
  r.sch->setCallsign("IZ0RPT");
  r.sch->setBeaconInterval(60000);
  r.sch->setMorseId(true);
  r.radio.failFsk = true;  // la radio non passa in FSK: nessun blocco, il testo e' uscito e il regista torna normale
  r.run(30000);
  CHECK(countBeacons(r) == 1 && r.sch->stats().morseAborted == 1 && r.sch->state() != TxScheduler::State::Morse);
  r.run(5000);
  CHECK(r.sch->state() == TxScheduler::State::Listen);
  // TX spenta dal sysop durante il Morse: si ferma
  CtlRig c;
  c.sch->setBeaconInterval(60000);
  c.sch->setMorseId(true);
  c.run(18000);
  CHECK(c.sch->state() == TxScheduler::State::Morse);
  c.sch->onControl({kSysopOff, 0, 1}, c.t, c.fx);
  CHECK(c.sch->state() != TxScheduler::State::Morse && !c.radio.inFsk && c.sch->stats().morseAborted == 1);
  c.run(5000);
  CHECK(!c.radio.inFsk);
}

static void testIdentifyCommandAlsoMorse() {
  CtlRig r;
  r.sch->setMorseId(true);
  r.run(500);
  r.sch->onControl({kSysopIdentify, 0, 1}, r.t, r.fx);
  CHECK(r.sch->state() == TxScheduler::State::Morse && r.radio.fskBegins == 1);
  r.run(12000);
  CHECK(r.sch->stats().morseIds == 1 && !r.radio.inFsk);
}

int main() {
  testMorseId();
  testMorseOffByDefault();
  testMorseAbortedByVoice();
  testMorseFailures();
  testIdentifyCommandAlsoMorse();
  testBeaconPeriodic();
  testBeaconOff();
  testBeaconWaitsForVoiceAndMessages();
  testStaleReply();
  testControlReplyAndOff();
  testControlOffMinutes();
  testControlStopsRelay();
  testControlIdentifyAndPower();
  testSafetyTripsOnLongVoice();
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
