#include "tx_scheduler.h"

#include <cstring>

#include "packet.h"
#include "protocol.h"

namespace lcr {

TxScheduler::TxScheduler(IRadio& radio, MessageStore& store, const Profile& voice, const Profile& message, uint32_t freqKhz, uint16_t sessionSeed)
    : radio_(radio), store_(store), voice_(voice), message_(message), freqKhz_(freqKhz), nextSession_(sessionSeed) {}

bool TxScheduler::switchTo(State s, uint32_t nowMs) {
  if (s == state_) return true;
  if (nowMs < nextParamMs_) return false;
  const bool toVoice = voiceParams(s);
  const bool wasVoice = voiceParams(state_);
  if (toVoice != wasVoice && !radio_.setParams(toVoice ? voice_ : message_, freqKhz_)) {
    stats_.paramErrors++;
    nextParamMs_ = nowMs + kParamRetryMs;
    return false;
  }
  state_ = s;
  return true;
}

void TxScheduler::setControl(RepeaterControl* control, const uint8_t key[kSysopKeyBytes]) {
  control_ = control;
  memcpy(key_, key, kSysopKeyBytes);
}

// Ogni trasmissione passa di qui: la radio spedisce il pacchetto e la sicurezza del ripetitore (trasmissione continua troppo lunga)
// ne tiene conto con il tempo in aria.
void TxScheduler::sendAndNote(const uint8_t* wire, size_t len, uint32_t nowMs, bool message) {
  radio_.send(wire, len);
  if (control_) control_->noteTx(nowMs, static_cast<uint32_t>(airtimeMs(len, message ? message_ : voice_) + 0.5), message);
}

// Trasmette subito un pacchetto con i parametri della voce (risposte e identificazione): interrompe un messaggio in corso, che
// ripartira' dall'inizio.
void TxScheduler::sendNow(const Packet& p, uint32_t nowMs, bool message) {
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(p, wire, sizeof(wire));
  if (m == 0) return;
  if (state_ == State::Morse) endMorse(true);
  if (state_ == State::Relay) abortRelay(nowMs, true);
  if (state_ == State::Listen || state_ == State::Relay) {  // Voice, Announce e Release hanno gia' i parametri della voce
    if (!radio_.setParams(voice_, freqKhz_)) {
      stats_.paramErrors++;
      state_ = State::Listen;
      return;
    }
  }
  state_ = State::Voice;
  annSent_ = 0;
  sendAndNote(wire, m, nowMs, message);
}

SysopResult TxScheduler::onControl(const SysopCommand& c, uint32_t nowMs, RepeaterControl::Effects& fx) {
  fx = RepeaterControl::Effects();
  if (!control_) return kSysopNotAllowed;
  uint32_t data = 0;
  const SysopResult r = control_->apply(c, nowMs, fx, data);
  if (fx.power >= 0) radio_.setPower(fx.power);
  if (control_->txAllowed(nowMs) || fx.forceReply) {
    sendNow(makeSysopReply(nextSession_++, callsign_, key_, {c.code, r, c.counter, data}), nowMs, false);
    stats_.replies++;
    if (fx.identify && control_->txAllowed(nowMs)) {
      Packet id = makeBeaconPacket(nextSession_++, callsign_);  // lo stesso identificativo di quello periodico
      sendNow(id, nowMs, false);
      startMorse(nowMs);  // e in Morse, se acceso
    }
  } else {
    stats_.repliesDropped++;
  }
  return r;
}

void TxScheduler::onStaleCommand(uint8_t code, uint32_t lastCounter, uint32_t nowMs) {
  if (!control_ || !control_->txAllowed(nowMs)) return;
  if (haveStaleReply_ && nowMs - lastStaleReplyMs_ < kStaleReplyGapMs) return;
  haveStaleReply_ = true;
  lastStaleReplyMs_ = nowMs;
  sendNow(makeSysopReply(nextSession_++, callsign_, key_, {code, kSysopCounterLow, lastCounter, control_->statusData(nowMs)}), nowMs, false);
  stats_.replies++;
}

bool TxScheduler::beaconDue(uint32_t nowMs) const {
  if (beaconIntervalMs_ == 0 || callsign_[0] == 0) return false;
  if (!haveBeacon_) return nowMs >= kBeaconFirstMs;
  return nowMs - lastBeaconMs_ >= beaconIntervalMs_;
}

void TxScheduler::setMorseId(bool on, int wpm, float toneHz, float devKhz) {
  morseOn_ = on;
  morseReq_ = RadioTestRequest();
  morseReq_.kind = RadioTestKind::Morse;
  morseReq_.fm = true;
  morseReq_.wpm = wpm;
  morseReq_.toneHz = toneHz;
  morseReq_.devKhz = devKhz;
}

// Dopo il testo dell'identificativo: la radio passa in FSK e trasmette il nominativo in Morse a tono. Un elemento alla volta, a ogni
// giro: un tono e' un pacchetto (blocca per la sua durata), una pausa e' solo un'attesa.
void TxScheduler::startMorse(uint32_t nowMs) {
  if (!morseOn_ || callsign_[0] == 0) return;
  strncpy(morseReq_.text, callsign_, sizeof(morseReq_.text) - 1);
  morseReq_.text[sizeof(morseReq_.text) - 1] = 0;
  morseN_ = planMorseBursts(morseReq_, morseBursts_, sizeof(morseBursts_) / sizeof(morseBursts_[0]), nullptr);
  if (morseN_ == 0) return;
  morseKbps_ = radioTestBitRateKbps(morseReq_);
  if (!radio_.fskBegin(static_cast<float>(freqKhz_) / 1000.0f, morseKbps_, morseReq_.devKhz, -1, 0)) {
    stats_.paramErrors++;
    radio_.fskEnd();  // torna ai parametri di prima, se qualcosa e' cambiato
    stats_.morseAborted++;
    return;
  }
  morseIdx_ = 0;
  morseStage_ = 0;
  morseNextMs_ = nowMs;
  state_ = State::Morse;
}

void TxScheduler::morseStep(uint32_t nowMs) {
  if (static_cast<int32_t>(nowMs - morseNextMs_) < 0) return;
  auto carrier = [&](uint32_t ms) {
    return sendCarrierMs(radio_, ms, morseKbps_, [&](uint32_t air) { if (control_) control_->noteTx(nowMs, air, false); });
  };
  if (morseStage_ == 0) {  // portante pura prima del primo tono: apre lo squelch
    morseStage_ = 1;
    if (!carrier(kMorseLeadMs)) endMorse(true);
    else morseNextMs_ = nowMs;
    return;
  }
  if (morseIdx_ >= morseN_) {
    if (morseStage_ == 1) {  // coda di portante, poi si chiude
      morseStage_ = 2;
      if (!carrier(kMorseTailMs)) endMorse(true);
      else morseNextMs_ = nowMs;
      return;
    }
    endMorse(false);
    return;
  }
  const MorseBurst& b = morseBursts_[morseIdx_++];
  if (!b.tone) {  // pausa: la portante continua
    if (!carrier(b.ms)) endMorse(true);
    else morseNextMs_ = nowMs;
    return;
  }
  if (!radio_.fskSend(radioTestPattern(morseReq_), b.bytes)) {
    endMorse(true);
    return;
  }
  if (control_) control_->noteTx(nowMs, static_cast<uint32_t>(burstAirMs(b.bytes, morseKbps_) + 0.5f), false);
  morseNextMs_ = nowMs;
}

// Fine dell'identificativo in Morse (completo, o interrotto): la radio torna LoRa ai parametri della voce, in ascolto.
void TxScheduler::endMorse(bool aborted) {
  radio_.fskEnd();
  state_ = State::Voice;
  if (aborted) stats_.morseAborted++;
  else stats_.morseIds++;
}

void TxScheduler::setCallsign(const char* callsign) {
  strncpy(callsign_, callsign ? callsign : "", kMaxCallsign);
  callsign_[kMaxCallsign] = 0;
}

bool TxScheduler::sendBusy(uint16_t seconds, uint32_t nowMs) {
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(makeBusyPacket(annSession_, callsign_, seconds), wire, sizeof(wire));
  if (m == 0) return false;
  sendAndNote(wire, m, nowMs, false);
  stats_.announces++;
  return true;
}

// Tempo in cui il ripetitore restera' occupato: aria dei pacchetti del messaggio piu' vecchio con i parametri del Messaggio, le pause
// tra i pacchetti, il cambio dei parametri e un po' di margine.
uint16_t TxScheduler::busySecondsForReady() const {
  const uint32_t ms = store_.readyAirtimeMs(message_) + static_cast<uint32_t>(store_.readyPackets()) * kInterPacketGapMs + 2000;
  const uint32_t sec = (ms + 999) / 1000;
  return static_cast<uint16_t>(sec > 65535 ? 65535 : sec);
}

// Annuncio prima della ritrasmissione (parametri della voce): due copie a kTextRepeatGapMs, poi si passa al Messaggio.
void TxScheduler::announceStep(uint32_t nowMs) {
  if (annSent_ == 0) {
    annSession_ = nextSession_++;
    annSeconds_ = busySecondsForReady();
  } else if (nowMs - lastAnnMs_ < kTextRepeatGapMs) {
    return;
  }
  if (annSent_ < 2) {
    sendBusy(annSeconds_, nowMs);
    annSent_++;
    lastAnnMs_ = nowMs;
    return;
  }
  if (!switchTo(State::Relay, nowMs)) return;
  session_ = nextSession_++;
  relayIdx_ = 0;
  haveRelayTx_ = false;
  busy_ = false;
  relayStep(nowMs);
}

// Annuncio di fine (0 secondi) con i parametri della voce, poi di nuovo in ascolto.
void TxScheduler::releaseStep(uint32_t nowMs) {
  if (annSent_ == 0) {
    annSession_ = nextSession_++;
  } else if (nowMs - lastAnnMs_ < kTextRepeatGapMs) {
    return;
  }
  if (annSent_ < 2) {
    sendBusy(0, nowMs);
    annSent_++;
    lastAnnMs_ = nowMs;
    return;
  }
  switchTo(State::Listen, nowMs);
}

void TxScheduler::abortRelay(uint32_t nowMs, bool byVoice) {
  relayIdx_ = 0;
  haveRelayTx_ = false;
  busy_ = false;
  if (byVoice) stats_.interrupted++;
  else stats_.busyAborts++;
  nextRelayMs_ = nowMs + kBetweenMsgMs;
}

void TxScheduler::listen(uint32_t nowMs) {
  uint8_t buf[kMaxWireBytes + 8];
  int rssi = 0;
  size_t n;
  while ((n = radio_.receive(buf, sizeof(buf), &rssi)) > 0) {
    Packet p;
    if (!decodePacket(buf, n, p)) continue;
    stats_.messageRx++;
    store_.onPacket(p, nowMs);
    // chi trasmette ha ancora pacchetti da mandare (parita' finale): non si parte sopra di lui
    if (nextRelayMs_ < nowMs + kBetweenMsgMs) nextRelayMs_ = nowMs + kBetweenMsgMs;
  }
}

void TxScheduler::tick(uint32_t nowMs, bool mayTransmit) {
  if (control_ && !control_->txAllowed(nowMs)) {  // spenta dal sysop, a tempo o dalla sicurezza: niente si trasmette
    mayTransmit = false;
    if (state_ == State::Morse) endMorse(true);
    if (state_ == State::Relay || state_ == State::Announce || state_ == State::Release) {
      const bool needSwitch = (state_ == State::Relay);
      relayIdx_ = 0;
      haveRelayTx_ = false;
      annSent_ = 0;
      if (!needSwitch || radio_.setParams(voice_, freqKhz_)) state_ = State::Voice;
    }
  }
  if (state_ != State::Voice && messages_) store_.tick(nowMs);
  if (state_ == State::Announce || state_ == State::Release) {
    if (queue_.size() > 0) {  // la voce interrompe anche gli annunci: restano gli stessi parametri
      state_ = State::Voice;
      annSent_ = 0;
    }
  }

  // 1. voce in coda: ha sempre la precedenza
  if (queue_.size() > 0) {
    if (state_ == State::Morse) endMorse(true);  // la radio torna ai parametri della voce
    if (state_ == State::Relay) abortRelay(nowMs, true);
    if (!switchTo(State::Voice, nowMs)) return;
    uint8_t pkt[kMaxWireBytes];
    size_t len = 0;
    if (queue_.next(nowMs, pkt, sizeof(pkt), len)) {
      if (mayTransmit) {
        sendAndNote(pkt, len, nowMs, false);  // bloccante: dura il tempo in aria del pacchetto
        stats_.voiceSent++;
        haveVoice_ = true;
        lastVoiceMs_ = nowMs;
      }
      queue_.sent(nowMs);
    }
    return;
  }

  // 1b. identificativo periodico: a canale libero (voce in coda: prima la voce; messaggio in ritrasmissione o in ricezione: si aspetta)
  if (mayTransmit && (state_ == State::Voice || state_ == State::Listen) && !store_.receiving() && beaconDue(nowMs)) {
    sendNow(makeBeaconPacket(nextSession_++, callsign_), nowMs, false);
    haveBeacon_ = true;
    lastBeaconMs_ = nowMs;
    stats_.beacons++;
    startMorse(nowMs);  // poi il nominativo in Morse a tono, se acceso
    return;
  }

  if (state_ == State::Morse) {
    morseStep(nowMs);
    return;
  }

  // 2. dopo la voce: si resta ai parametri della voce per un po', poi si ascolta
  if (state_ == State::Announce) {
    announceStep(nowMs);
    return;
  }
  if (state_ == State::Release) {
    releaseStep(nowMs);
    return;
  }
  if (state_ == State::Voice) {
    if (messages_ && (!haveVoice_ || nowMs - lastVoiceMs_ >= kHoldMs)) switchTo(State::Listen, nowMs);
    return;
  }

  // 3. ascolto o ritrasmissione
  if (state_ == State::Listen) {
    listen(nowMs);
    if (store_.receiving() || store_.readyCount() == 0 || !mayTransmit) return;
    if (!tailElapsed(nowMs) || nowMs < nextRelayMs_) return;
    if (radio_.channelBusy()) {  // qualcuno sta trasmettendo sull'uscita: si riprova tra poco
      nextRelayMs_ = nowMs + kBusyRetryMs;
      return;
    }
    if (callsign_[0]) {  // annuncio di occupato con i parametri della voce
      if (!switchTo(State::Announce, nowMs)) return;
      annSent_ = 0;
      announceStep(nowMs);
      return;
    }
    session_ = nextSession_++;
    relayIdx_ = 0;
    haveRelayTx_ = false;
    busy_ = false;
    state_ = State::Relay;  // stessi parametri dell'ascolto: nessun cambio radio
    relayStep(nowMs);
    return;
  }
  relayStep(nowMs);
}

void TxScheduler::relayStep(uint32_t nowMs) {
  if (haveRelayTx_ && nowMs - lastRelayTxMs_ < kInterPacketGapMs) return;
  {  // controllo del canale prima di ogni pacchetto
    if (radio_.channelBusy()) {
      if (!busy_) {
        busy_ = true;
        busySinceMs_ = nowMs;
      }
      if (nowMs - busySinceMs_ >= kBusyAbortMs) {
        abortRelay(nowMs, false);
        endRelayState(nowMs);
      }
      return;
    }
    busy_ = false;
  }
  Packet p;
  uint8_t wire[kMaxWireBytes];
  size_t m = 0;
  if (store_.readyPacket(relayIdx_, session_, p)) {
    p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);
    m = encodePacket(p, wire, sizeof(wire));
  }
  if (m > 0) {
    sendAndNote(wire, m, nowMs, true);
    stats_.relayPackets++;
    haveRelayTx_ = true;
    lastRelayTxMs_ = nowMs;
  }
  relayIdx_++;
  if (relayIdx_ >= store_.readyPackets()) {
    store_.popReady();
    stats_.relayed++;
    relayIdx_ = 0;
    haveRelayTx_ = false;
    nextRelayMs_ = nowMs + kBetweenMsgMs;
    endRelayState(nowMs);
  }
}

// Fine della ritrasmissione (completa o interrotta dal canale occupato): con l'annuncio acceso si libera il display delle stazioni.
void TxScheduler::endRelayState(uint32_t nowMs) {
  annSent_ = 0;
  if (callsign_[0] && switchTo(State::Release, nowMs)) return;
  state_ = State::Listen;
}

}  // namespace lcr
