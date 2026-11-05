#include "voice_node.h"

#include <cstring>

namespace lcr {

VoiceNode::VoiceNode(const NodeConfig& cfg, IRadio& radio, ICodec& codec, IAudioIn& in, IAudioOut& out,
                     INotifier& notify)
    : cfg_(cfg), radio_(radio), codec_(codec), in_(in), out_(out), notify_(notify) {}

uint16_t VoiceNode::makeSessionId() {
  if (cfg_.newSessionId) return cfg_.newSessionId();
  return fallbackSession_++;
}

void VoiceNode::pttPressed(uint32_t nowMs) {
  if (state_ == State::Transmitting) {
    notify_.pttDenied(DenyReason::Transmitting);
    return;
  }
  if (state_ == State::Receiving) {
    clickCount_ = 0;
    notify_.pttDenied(DenyReason::Receiving);
    return;
  }
  // A riposo la pressione e' un clic oppure l'inizio di una trasmissione: lo si capisce da quanto dura.
  if (clickCount_ > 0 && nowMs - lastClickMs_ > kPttClickGapMs) clickCount_ = 0;
  if (clickCount_ >= 2) {
    // Terzo clic veloce: attiva o disattiva la prova di collegamento, senza trasmettere.
    clickCount_ = 0;
    testMode_ = !testMode_;
    notify_.testModeChanged(testMode_);
    return;
  }
  pendingStart_ = true;
  pressMs_ = nowMs;
  // Il microfono si bufferizza subito, cosi' l'inizio della frase non si perde.
  if (!testMode_ && !micStarted_) {
    in_.start();
    micStarted_ = true;
  }
}

void VoiceNode::beginPtt(uint32_t nowMs) {
  // Senza un nominativo reale (N0CALL) non si trasmette: la ricezione funziona.
  if (isPlaceholderCallsign(cfg_.callsign)) {
    stopMic();
    notify_.pttDenied(DenyReason::NoCallsign);
    return;
  }
  if (state_ == State::Receiving) {
    stopMic();
    notify_.pttDenied(DenyReason::Receiving);
    return;
  }
  if (radio_.channelBusy()) {
    stopMic();
    notify_.pttDenied(DenyReason::ChannelBusy);
    return;
  }
  startTx(nowMs);
}

void VoiceNode::stopMic() {
  if (micStarted_) {
    in_.stop();
    micStarted_ = false;
  }
}

void VoiceNode::startTx(uint32_t nowMs) {
  const uint16_t sid = makeSessionId();
  txTest_ = testMode_;
  testFramesSent_ = 0;
  nextTestFrameMs_ = nowMs;
  if (txTest_) {
    lastTestSession_ = sid;
    lastTestValid_ = true;
  }
  replyPending_ = false;
  tx_ = TxSession(cfg_.callsign, sid, cfg_.fec, txTest_ ? kModeTest : kModeCodec2_1200);
  framesCaptured_ = 0;
  releasing_ = false;
  limitHit_ = false;
  justSent_ = false;
  nextSendMs_ = nowMs;
  state_ = State::Transmitting;
  // La bufferizzazione del microfono parte subito; il primo pacchetto
  // (nominativo) viene inviato al primo tick mentre l'audio si accumula.
  if (!txTest_ && !micStarted_) {  // nella prova non serve il microfono
    in_.start();
    micStarted_ = true;
  }
  tx_.begin();
  notify_.txStarted();
}

void VoiceNode::pttReleased(uint32_t nowMs) {
  if (pendingStart_) {
    // Pressione breve: un clic. Non si e' trasmesso nulla; tre clic veloci cambiano il modo prova.
    pendingStart_ = false;
    stopMic();
    if (clickCount_ < 2) clickCount_++;
    lastClickMs_ = nowMs;
    return;
  }
  if (state_ == State::Transmitting && !releasing_) finishCapture(false);
}

void VoiceNode::finishCapture(bool limit) {
  stopMic();
  tx_.finish();
  releasing_ = true;
  limitHit_ = limit;
}

void VoiceNode::tick(uint32_t nowMs) {
  // PTT rimasto premuto oltre la durata di un clic: inizia la trasmissione (o il rifiuto, con il motivo).
  if (pendingStart_ && static_cast<int32_t>(nowMs - pressMs_) >= static_cast<int32_t>(kPttClickMaxMs)) {
    pendingStart_ = false;
    clickCount_ = 0;
    beginPtt(nowMs);
  }
  if (state_ == State::Transmitting) {
    txTick(nowMs);
  } else {
    rxTick(nowMs);
    if (replyPending_ && state_ == State::Idle && static_cast<int32_t>(nowMs - replyAtMs_) >= 0) sendTestReply(nowMs);
  }
}

void VoiceNode::txTick(uint32_t nowMs) {
  // radio_.send() e' bloccante: al tick successivo la trasmissione e' finita, da qui conta la pausa.
  if (justSent_) {
    justSent_ = false;
    nextSendMs_ = nowMs + kInterPacketGapMs;
  }
  if (!releasing_ && txTest_) {
    // Prova: un frame noto ogni 40 ms, come se arrivasse dal codec, fino a kTestFrames.
    for (int guard = 0; guard < 8 && !releasing_ && testFramesSent_ < kTestFrames &&
                        static_cast<int32_t>(nowMs - nextTestFrameMs_) >= 0;
         guard++) {
      uint8_t frame[kFrameBytes];
      testFramePattern(lastTestSession_, testFramesSent_, frame);
      if (!tx_.pushFrame(frame)) {
        finishCapture(true);
        break;
      }
      testFramesSent_++;
      nextTestFrameMs_ += kFrameMs;
    }
    if (!releasing_ && testFramesSent_ >= kTestFrames) finishCapture(false);
  } else if (!releasing_) {
    int16_t pcm[kFrameSamples];
    uint8_t frame[kFrameBytes];
    for (int guard = 0; guard < 8 && !releasing_ && in_.readFrame(pcm); guard++) {
      if (framesCaptured_ >= kMaxTurnFrames) {
        finishCapture(true);
        break;
      }
      codec_.encode(pcm, frame);
      if (!tx_.pushFrame(frame)) {
        finishCapture(true);
        break;
      }
      framesCaptured_++;
    }
    if (!releasing_ && framesCaptured_ >= kMaxTurnFrames) finishCapture(true);
  }

  Packet p;
  const bool gapOver = static_cast<int32_t>(nowMs - nextSendMs_) >= 0;
  if (gapOver && tx_.popPacket(p)) {
    uint8_t buf[kMaxWireBytes];
    const size_t n = encodePacket(p, buf, sizeof(buf));
    if (n) {
      radio_.send(buf, n);
      justSent_ = true;
    }
  } else if (releasing_ && !tx_.hasPacket()) {
    radio_.startReceive();
    state_ = State::Idle;
    notify_.txEnded(limitHit_);
  }
}

void VoiceNode::rxTick(uint32_t nowMs) {
  uint8_t buf[kMaxWireBytes];
  for (int i = 0; i < 4; i++) {
    int rssi = 0;
    const size_t n = radio_.receive(buf, sizeof(buf), &rssi);
    if (n == 0) break;
    Packet p;
    if (!decodePacket(buf, n, p)) continue;
    if (p.mode == kModeTestReport) {
      // Risposta alla prova trasmessa da qui: 12 byte con il risultato misurato dall'altra stazione.
      TestReport r;
      if (lastTestValid_ && p.session == lastTestSession_ && p.payloadLen == kTestReportBytes &&
          decodeTestReport(p.payload, p.payloadLen, r)) {
        lastTestValid_ = false;  // una sola risposta per prova
        notify_.linkTestReport(true, p.callsign, r);
      }
      continue;
    }
    if (p.mode != kModeCodec2_1200 && p.mode != kModeTest) continue;  // modi sconosciuti
    if (state_ == State::Receiving && (p.mode == kModeTest) != rxIsTest_) continue;  // niente mescolanze
    if (!rx_.onPacket(p, nowMs)) continue;
    if (p.mode == kModeTest) linkRx_.onPacket(rssi, radio_.lastSnr(), radio_.hasSnr());
    if (rx_.session() != notifiedSession_) {
      notifiedSession_ = rx_.session();
      callsignNotified_ = false;
    }
    if (state_ == State::Idle) {
      rxIsTest_ = p.mode == kModeTest;
      if (rxIsTest_) {
        linkRx_.begin(p.session);
        linkRx_.onPacket(rssi, radio_.lastSnr(), radio_.hasSnr());
      }
      state_ = State::Receiving;
      nextPlayMs_ = nowMs;
      notify_.rxStarted();
    }
  }

  if (state_ != State::Receiving) return;

  if (!callsignNotified_ && rx_.callsign()[0]) {
    callsignNotified_ = true;
    notify_.rxCallsign(rx_.callsign());
  }

  if (nowMs < nextPlayMs_) return;
  const RxStream::Out o = rx_.nextFrame(nowMs);
  switch (o.kind) {
    case RxStream::Kind::Wait:
      nextPlayMs_ = nowMs;
      return;
    case RxStream::Kind::Frame: {
      if (rxIsTest_) {
        linkRx_.onFrame(o.frame);  // prova: confronto con la sequenza attesa, nessun audio
        break;
      }
      int16_t pcm[kFrameSamples];
      codec_.decode(o.frame, pcm);
      out_.playFrame(pcm);
      break;
    }
    case RxStream::Kind::Lost:
      if (rxIsTest_) {
        linkRx_.onLost();
      } else {
        out_.playLost();
      }
      break;
    case RxStream::Kind::End:
      state_ = State::Idle;
      if (rxIsTest_) {
        finishTestRx(nowMs);
      } else {
        notify_.rxEnded(rx_.callsign(), rx_.lostFrames(), rx_.recoveredPackets());
      }
      return;
  }
  nextPlayMs_ += kFrameMs;
  if (nowMs > nextPlayMs_ + 5 * kFrameMs) nextPlayMs_ = nowMs + kFrameMs;
}

void VoiceNode::finishTestRx(uint32_t nowMs) {
  const uint32_t rec = rx_.recoveredPackets();
  const TestReport r = linkRx_.report(static_cast<uint8_t>(rec > 255 ? 255 : rec));
  notify_.linkTestReport(false, rx_.callsign(), r);
  // La risposta parte solo con la prova attiva, e mai senza un nominativo vero.
  if (testMode_ && !isPlaceholderCallsign(cfg_.callsign)) {
    replyPending_ = true;
    replyAtMs_ = nowMs + kTestReplyDelayMs;  // lascia a chi ha trasmesso il tempo di tornare in ascolto
    replyTries_ = 0;
    replySession_ = linkRx_.session();
    replyReport_ = r;
  }
}

void VoiceNode::sendTestReply(uint32_t nowMs) {
  if (radio_.channelBusy()) {
    if (++replyTries_ >= 5) replyPending_ = false;  // canale occupato: si rinuncia
    replyAtMs_ = nowMs + 200;
    return;
  }
  replyPending_ = false;
  Packet p;
  p.mode = kModeTestReport;
  p.session = replySession_;
  p.seq = 0;
  p.flags = kFlagStart | kFlagEnd;
  strncpy(p.callsign, cfg_.callsign, kMaxCallsign);
  p.payloadLen = kTestReportBytes;
  encodeTestReport(replyReport_, p.payload);
  uint8_t buf[kMaxWireBytes];
  const size_t n = encodePacket(p, buf, sizeof(buf));
  if (n) {
    radio_.send(buf, n);
    radio_.startReceive();
  }
}

}  // namespace lcr
