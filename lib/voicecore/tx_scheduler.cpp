#include "tx_scheduler.h"

#include "packet.h"
#include "protocol.h"

namespace lcr {

TxScheduler::TxScheduler(IRadio& radio, MessageStore& store, const Profile& voice, const Profile& message, uint32_t freqKhz, uint16_t sessionSeed)
    : radio_(radio), store_(store), voice_(voice), message_(message), freqKhz_(freqKhz), nextSession_(sessionSeed) {}

bool TxScheduler::switchTo(State s, uint32_t nowMs) {
  if (s == state_) return true;
  if (nowMs < nextParamMs_) return false;
  const bool toVoice = (s == State::Voice);
  const bool wasVoice = (state_ == State::Voice);
  if (toVoice != wasVoice && !radio_.setParams(toVoice ? voice_ : message_, freqKhz_)) {
    stats_.paramErrors++;
    nextParamMs_ = nowMs + kParamRetryMs;
    return false;
  }
  state_ = s;
  return true;
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
  if (haveRelayTx_ || relayIdx_ > 0) {  // il primo pacchetto e' gia' passato dal CAD in Listen
    if (radio_.channelBusy()) {
      if (!busy_) {
        busy_ = true;
        busySinceMs_ = nowMs;
      }
      if (nowMs - busySinceMs_ >= kBusyAbortMs) {
        abortRelay(nowMs, false);
        state_ = State::Listen;
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
    state_ = State::Listen;
  }
}

}  // namespace lcr
