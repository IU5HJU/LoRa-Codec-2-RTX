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

void TxScheduler::setCallsign(const char* callsign) {
  strncpy(callsign_, callsign ? callsign : "", kMaxCallsign);
  callsign_[kMaxCallsign] = 0;
}

bool TxScheduler::sendBusy(uint16_t seconds) {
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(makeBusyPacket(annSession_, callsign_, seconds), wire, sizeof(wire));
  if (m == 0) return false;
  radio_.send(wire, m);
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
    sendBusy(annSeconds_);
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
    sendBusy(0);
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
  if (state_ != State::Voice && messages_) store_.tick(nowMs);
  if (state_ == State::Announce || state_ == State::Release) {
    if (queue_.size() > 0) {  // la voce interrompe anche gli annunci: restano gli stessi parametri
      state_ = State::Voice;
      annSent_ = 0;
    }
  }

  // 1. voce in coda: ha sempre la precedenza
  if (queue_.size() > 0) {
    if (state_ == State::Relay) abortRelay(nowMs, true);
    if (!switchTo(State::Voice, nowMs)) return;
    uint8_t pkt[kMaxWireBytes];
    size_t len = 0;
    if (queue_.next(nowMs, pkt, sizeof(pkt), len)) {
      if (mayTransmit) {
        radio_.send(pkt, len);  // bloccante: dura il tempo in aria del pacchetto
        stats_.voiceSent++;
        haveVoice_ = true;
        lastVoiceMs_ = nowMs;
      }
      queue_.sent(nowMs);
    }
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
    radio_.send(wire, m);
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
