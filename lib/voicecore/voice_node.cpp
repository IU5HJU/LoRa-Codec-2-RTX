#include "voice_node.h"

#include <cstdio>
#include <cstring>

#include "packet.h"

namespace lcr {

VoiceNode::VoiceNode(const NodeConfig& cfg, IRadio& radio, ICodec& codec, IAudioIn& in, IAudioOut& out,
                     INotifier& notify)
    : cfg_(cfg),
      prof_(profileFor(cfg.profile)),
      maxTurnFrames_(prof_.maxTurnMs / kFrameMs),
      radio_(radio),
      codec_(codec),
      in_(in),
      out_(out),
      notify_(notify) {
  RxStream::Config rc;
  rc.prebufferFrames = prof_.rxPrebufferFrames;
  rc.prebufferTimeoutMs = prof_.rxPrebufferTimeoutMs;
  rc.sessionTimeoutMs = prof_.rxSessionTimeoutMs;
  rx_.configure(rc);
  recMode_ = prof_.message;  // registra e poi invia: il messaggio si trasmette solo a registrazione conclusa
}

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
  // Un messaggio aperto (voce in riproduzione o testo in mostra) si chiude con la pressione, senza trasmettere.
  if (playing_ || textOpen_) {
    closeMessage();
    return;
  }
  // A riposo la pressione e' un clic oppure l'inizio di una trasmissione: lo si capisce da quanto dura.
  if (clickCount_ > 0 && nowMs - lastClickMs_ > kPttClickGapMs) clickCount_ = 0;
  if (clickCount_ >= 2 && !prof_.message && !meshActive()) {
    // Terzo clic veloce: attiva o disattiva la prova di collegamento, senza trasmettere.
    clickCount_ = 0;
    testMode_ = !testMode_;
    notify_.testModeChanged(testMode_);
    return;
  }
  pendingStart_ = true;
  pressMs_ = nowMs;
  // Il microfono si bufferizza subito, cosi' l'inizio della frase non si perde.
  if (!testMode_ && !micStarted_ && !meshActive()) {
    in_.start();
    micStarted_ = true;
  }
}

void VoiceNode::beginPtt(uint32_t nowMs) {
  // Durante l'escursione sulla rete di testo il PTT non trasmette voce (i testi si scrivono dal menu).
  if (meshActive()) {
    stopMic();
    notify_.pttDenied(DenyReason::TextOnly);
    return;
  }
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
  const bool rec = recMode_ && !testMode_;
  recCount_ = recSent_ = 0;
  txTotal_ = txSent_ = 0;
  testFramesSent_ = 0;
  nextTestFrameMs_ = nowMs;
  if (txTest_) {
    lastTestSession_ = sid;
    lastTestValid_ = true;
  }
  replyPending_ = false;
  tx_ = TxSession(cfg_.callsign, sid, cfg_.fec, txTest_ ? kModeTest : prof_.codecMode);
  if (cfg_.sendPosition && !txTest_) tx_.setPosition(cfg_.position);
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
  if (rec) {
    setRecPhase(RecPhase::Recording);  // nessun pacchetto finche' il PTT non e' rilasciato
  } else {
    tx_.begin();
  }
  notify_.txStarted();
}

void VoiceNode::setRecPhase(RecPhase p) {
  recPhase_ = p;
  notify_.recPhaseChanged(static_cast<int>(p));
}

// Fine della registrazione (PTT rilasciato o limite di 30 s): il microfono si ferma, la trasmissione parte tra 1 s.
void VoiceNode::finishRecording(uint32_t nowMs, bool limit) {
  stopMic();
  limitHit_ = limit;
  recReleaseMs_ = nowMs;
  setRecPhase(RecPhase::Waiting);
}

void VoiceNode::recTick(uint32_t nowMs) {
  switch (recPhase_) {
    case RecPhase::Recording: {
      int16_t pcm[kFrameSamples];
      for (int guard = 0; guard < 8 && recPhase_ == RecPhase::Recording && in_.readFrame(pcm); guard++) {
        if (recCount_ >= maxTurnFrames_) {
          finishRecording(nowMs, true);
          break;
        }
        codec_.encode(pcm, recBuf_ + recCount_ * frameBytesFor(prof_.codecMode));
        recCount_++;
      }
      if (recPhase_ == RecPhase::Recording && recCount_ >= maxTurnFrames_) finishRecording(nowMs, true);
      break;
    }
    case RecPhase::Waiting:
      if (static_cast<int32_t>(nowMs - recReleaseMs_) >= static_cast<int32_t>(kRecSendDelayMs)) {
        // Il messaggio e' gia' registrato: se ne conosce la lunghezza e la si dichiara nel pacchetto START.
        txTotal_ = TxSession::expectedPackets(recCount_, cfg_.fec);
        if (txTotal_ > 255) txTotal_ = 0;
        tx_.setExpectedTotal(txTotal_);
        tx_.begin();
        if (txTotal_) notify_.txProgress(1);  // l'invio parte: 1% subito, cosi' non sembra bloccato
        nextSendMs_ = nowMs;
        setRecPhase(RecPhase::Sending);
      }
      break;
    case RecPhase::Sending:
      // Frame verso la coda dei pacchetti, senza riempirla: ogni frame puo' produrre dati e parita'.
      for (int guard = 0; guard < 24 && !releasing_ && recSent_ < recCount_ && tx_.queued() <= 4; guard++) {
        if (!tx_.pushFrame(recBuf_ + recSent_ * frameBytesFor(prof_.codecMode))) {
          limitHit_ = true;
          recSent_ = recCount_;
          break;
        }
        recSent_++;
      }
      if (!releasing_ && recSent_ >= recCount_) {
        tx_.finish();
        releasing_ = true;
      }
      break;
    case RecPhase::None: break;
  }
}

void VoiceNode::pttReleased(uint32_t nowMs) {
  if (pendingStart_) {
    // Pressione breve: un clic. Non si e' trasmesso nulla; tre clic veloci cambiano il modo prova.
    pendingStart_ = false;
    stopMic();
    if (inbox_.has()) {  // c'e' un messaggio in attesa: il clic lo apre
      clickCount_ = 0;
      openMessage(nowMs);
      return;
    }
    if (clickCount_ < 2) clickCount_++;
    lastClickMs_ = nowMs;
    return;
  }
  if (recPhase_ == RecPhase::Recording) {
    finishRecording(nowMs, false);
    return;
  }
  if (recPhase_ != RecPhase::None) return;  // registrazione gia' conclusa: il rilascio non cambia nulla
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
    playTick(nowMs);
    if (guest_ && state_ == State::Idle && static_cast<int32_t>(nowMs - guestUntilMs_) >= 0) endMeshcom();
    if (meshActive()) meshAckTick(nowMs);
    rxTick(nowMs);
    if (replyPending_ && state_ == State::Idle && static_cast<int32_t>(nowMs - replyAtMs_) >= 0) sendTestReply(nowMs);
  }
}

void VoiceNode::txTick(uint32_t nowMs) {
  if (textTx_) {  // testo: due copie con la stessa sessione, a distanza di kTextRepeatGapMs
    if (justSent_) {
      justSent_ = false;
      nextSendMs_ = nowMs + kTextRepeatGapMs + cfg_.extraGapMs;
    }
    if (textCopies_ > 0) {
      if (static_cast<int32_t>(nowMs - nextSendMs_) >= 0) {
        radio_.send(textWire_, textWireLen_);
        justSent_ = true;
        textCopies_--;
      }
    } else {
      justSent_ = false;
      textTx_ = false;
      radio_.startReceive();
      state_ = State::Idle;
      notify_.txEnded(false);
    }
    return;
  }
  // radio_.send() e' bloccante: al tick successivo la trasmissione e' finita, da qui conta la pausa.
  if (justSent_) {
    justSent_ = false;
    nextSendMs_ = nowMs + kInterPacketGapMs + cfg_.extraGapMs;
  }
  if (recPhase_ == RecPhase::Recording || recPhase_ == RecPhase::Waiting) {
    recTick(nowMs);
    return;  // finche' si registra o si attende non si trasmette niente
  }
  if (recPhase_ == RecPhase::Sending) {
    recTick(nowMs);
  } else if (!releasing_ && txTest_) {
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
    uint8_t frame[kMaxFrameBytes];
    for (int guard = 0; guard < 8 && !releasing_ && in_.readFrame(pcm); guard++) {
      if (framesCaptured_ >= maxTurnFrames_) {
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
    if (!releasing_ && framesCaptured_ >= maxTurnFrames_) finishCapture(true);
  }

  Packet p;
  const bool gapOver = static_cast<int32_t>(nowMs - nextSendMs_) >= 0;
  if (gapOver && tx_.popPacket(p)) {
    uint8_t buf[kMaxWireBytes];
    const size_t n = encodePacket(p, buf, sizeof(buf));
    if (n) {
      radio_.send(buf, n);
      justSent_ = true;
      if (txTotal_) {
        txSent_++;
        int pct = static_cast<int>(txSent_ * 100 / txTotal_);
        notify_.txProgress(pct < 1 ? 1 : (pct > 100 ? 100 : pct));
      }
    }
  } else if (releasing_ && !tx_.hasPacket()) {
    radio_.startReceive();
    state_ = State::Idle;
    if (recPhase_ != RecPhase::None) setRecPhase(RecPhase::None);
    notify_.txEnded(limitHit_);
  }
}

void VoiceNode::rxTick(uint32_t nowMs) {
  uint8_t buf[kMaxRxBytes];
  for (int i = 0; i < 4; i++) {
    int rssi = 0;
    const size_t n = radio_.receive(buf, sizeof(buf), &rssi);
    if (n == 0) break;
    if (meshActive()) {  // escursione MeshCom: frame di un'altra rete, non pacchetti del progetto
      handleMeshFrame(buf, n, nowMs);
      continue;
    }
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
    if (p.mode == kModeText) {
      handleText(p);
      continue;
    }
    if (p.mode != prof_.codecMode && p.mode != kModeTest) continue;  // modi sconosciuti
    if (state_ == State::Receiving && (p.mode == kModeTest) != rxIsTest_) continue;  // niente mescolanze
    if (!rx_.onPacket(p, nowMs)) continue;
    if (p.mode == kModeTest) linkRx_.onPacket(rssi, radio_.lastSnr(), radio_.hasSnr());
    if (rx_.session() != notifiedSession_) {
      notifiedSession_ = rx_.session();
      callsignNotified_ = false;
      positionNotified_ = false;
    }
    if (state_ == State::Idle) {
      rxIsTest_ = p.mode == kModeTest;
      if (rxIsTest_) {
        linkRx_.begin(p.session);
        linkRx_.onPacket(rssi, radio_.lastSnr(), radio_.hasSnr());
      }
      if (!rxIsTest_) {  // una nuova ricezione interrompe la riproduzione o la lettura (il messaggio resta in casella)
        playing_ = false;
        textOpen_ = false;
      }
      state_ = State::Receiving;
      nextPlayMs_ = nowMs;
      playbackNotified_ = false;
      rxPercent_ = 0;
      notify_.rxStarted();
    }
    reportRxProgress();
  }

  if (state_ != State::Receiving) return;

  if (!callsignNotified_ && rx_.callsign()[0]) {
    callsignNotified_ = true;
    notify_.rxCallsign(rx_.callsign());
  }
  if (!positionNotified_ && rx_.position().valid) {
    positionNotified_ = true;
    notify_.rxPosition(rx_.position());
  }

  if (prof_.message && !rxIsTest_) {  // profilo Messaggio: la voce va in casella, non si riproduce
    drainToInbox(nowMs);
    return;
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
      if (!playbackNotified_) {
        playbackNotified_ = true;
        if (rx_.totalPackets() && rxPercent_ < 100) {  // messaggio completo (anche se l'ultimo pacchetto si e' perso)
          rxPercent_ = 100;
          notify_.rxProgress(100);
        }
        notify_.rxPlaybackStarted();
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
        if (!playbackNotified_) {
          playbackNotified_ = true;
          notify_.rxPlaybackStarted();
        }
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

// Profilo Messaggio: a ricezione conclusa i frame (anche quelli persi) vanno in casella, senza pause; il messaggio vecchio
// viene sostituito. Poi la casella avvisa che c'e' un messaggio da ascoltare.
void VoiceNode::drainToInbox(uint32_t nowMs) {
  for (uint32_t guard = 0; guard < kMaxTurnFrames + 16; guard++) {
    const RxStream::Out o = rx_.nextFrame(nowMs);
    if (o.kind == RxStream::Kind::Wait) return;
    if (o.kind == RxStream::Kind::End) {
      state_ = State::Idle;
      storing_ = false;
      const bool got = inbox_.commitVoice(rx_.callsign());
      notify_.rxEnded(rx_.callsign(), rx_.lostFrames(), rx_.recoveredPackets());
      if (got) notify_.messageReceived(false, inbox_.callsign());
      return;
    }
    if (!storing_) {
      storing_ = true;
      inbox_.beginVoice();
      if (rx_.totalPackets() && rxPercent_ < 100) {
        rxPercent_ = 100;
        notify_.rxProgress(100);
      }
    }
    if (o.kind == RxStream::Kind::Frame) inbox_.pushFrame(o.frame); else inbox_.pushLost();
  }
}

// Testo ricevuto: un solo pacchetto, mandato due volte dal mittente (la seconda copia si scarta). Sostituisce il messaggio
// in casella.
void VoiceNode::handleText(const Packet& p) {
  if (lastTextValid_ && p.session == lastTextSession_ && strncmp(p.callsign, lastTextCall_, kMaxCallsign) == 0) return;
  if (!inbox_.setText(p.callsign, reinterpret_cast<const char*>(p.payload), p.payloadLen)) return;
  lastTextValid_ = true;
  lastTextSession_ = p.session;
  strncpy(lastTextCall_, p.callsign, kMaxCallsign);
  lastTextCall_[kMaxCallsign] = 0;
  playing_ = false;
  textOpen_ = false;
  notify_.messageReceived(true, p.callsign);
}

void VoiceNode::openMessage(uint32_t nowMs) {
  if (inbox_.kind() == Inbox::Kind::Voice) {
    playing_ = true;
    playIdx_ = 0;
    nextPlayMs_ = nowMs;
    notify_.messageOpened(false);
  } else if (inbox_.kind() == Inbox::Kind::Text) {
    textOpen_ = true;
    notify_.messageOpened(true);
  }
}

// Messaggio ascoltato, letto o interrotto: si cancella.
void VoiceNode::closeMessage() {
  playing_ = false;
  textOpen_ = false;
  inbox_.clear();
  notify_.messageClosed();
}

void VoiceNode::playTick(uint32_t nowMs) {
  if (!playing_ || static_cast<int32_t>(nowMs - nextPlayMs_) < 0) return;
  if (playIdx_ >= inbox_.frames()) {
    closeMessage();
    return;
  }
  uint8_t frame[kFrameBytes];
  int16_t pcm[kFrameSamples];
  if (inbox_.frameAt(playIdx_, frame)) {
    codec_.decode(frame, pcm);
    out_.playFrame(pcm);
  } else {
    out_.playLost();
  }
  playIdx_++;
  nextPlayMs_ += kFrameMs;
  if (nowMs > nextPlayMs_ + 5 * kFrameMs) nextPlayMs_ = nowMs + kFrameMs;
}

bool VoiceNode::sendText(const char* text, uint32_t nowMs) {
  const size_t len = text ? strlen(text) : 0;
  if (!isValidText(text, len)) return false;
  if (meshActive()) {  // durante l'escursione i testi partono dal menu MeshCom (startMeshcom)
    notify_.pttDenied(DenyReason::TextOnly);
    return false;
  }
  if (isPlaceholderCallsign(cfg_.callsign) || !isValidCallsign(cfg_.callsign)) {
    notify_.pttDenied(DenyReason::NoCallsign);
    return false;
  }
  if (state_ == State::Transmitting) {
    notify_.pttDenied(DenyReason::Transmitting);
    return false;
  }
  if (state_ == State::Receiving) {
    notify_.pttDenied(DenyReason::Receiving);
    return false;
  }
  if (radio_.channelBusy()) {
    notify_.pttDenied(DenyReason::ChannelBusy);
    return false;
  }
  Packet p;
  p.mode = kModeText;
  p.session = makeSessionId();
  p.seq = 0;
  p.flags = kFlagStart | kFlagEnd;
  strncpy(p.callsign, cfg_.callsign, kMaxCallsign);
  p.payloadLen = static_cast<uint8_t>(len);
  memcpy(p.payload, text, len);
  textWireLen_ = encodePacket(p, textWire_, sizeof(textWire_));
  if (!textWireLen_) return false;
  playing_ = false;
  textOpen_ = false;
  textTx_ = true;
  textCopies_ = 2;
  justSent_ = false;
  nextSendMs_ = nowMs;
  state_ = State::Transmitting;
  notify_.txStarted();
  return true;
}

// ---- Escursione MeshCom: testi compatibili con la rete a pacchetto (meshcom_frame.h) ----

namespace {
bool equalsNoCase(const char* a, const char* b) {
  for (; *a && *b; a++, b++) {
    char x = *a, y = *b;
    if (x >= 'a' && x <= 'z') x = static_cast<char>(x - 32);
    if (y >= 'a' && y <= 'z') y = static_cast<char>(y - 32);
    if (x != y) return false;
  }
  return *a == *b;
}
}  // namespace

uint32_t VoiceNode::makeMsgId() {
  uint32_t id = cfg_.newMsgId ? cfg_.newMsgId() : ++meshIdCounter_;
  return id ? id : 1;
}

bool VoiceNode::meshSeen(uint32_t id, size_t* slot) {
  for (size_t i = 0; i < kMeshSeenSlots; i++) {
    if (meshSeen_[i].used && meshSeen_[i].id == id) {
      if (slot) *slot = i;
      return true;
    }
  }
  return false;
}

void VoiceNode::meshRemember(uint32_t id, uint32_t nowMs) {
  (void)nowMs;
  MeshSeen& s = meshSeen_[meshNext_];
  s.used = true;
  s.id = id;
  s.acked = false;
  s.ackedAtMs = 0;
  meshNext_ = (meshNext_ + 1) % kMeshSeenSlots;
}

// Un frame arrivato dalla radio durante l'escursione MeshCom. Si leggono solo i testi (':') con la FCS giusta, mai visti prima (le
// copie ripetute dai nodi hanno lo stesso ID), diretti al nostro nominativo-SSID. I messaggi di servizio ("{CET}..." e simili)
// e le conferme di ricezione dei nostri testi non si mostrano. Un "{NNN" in fondo chiede la conferma: si risponde con
// "MITTENTE:ackNNN".
void VoiceNode::handleMeshFrame(const uint8_t* b, size_t n, uint32_t nowMs) {
  // Diagnostica (meshLog): una riga per ogni frame ricevuto durante l'escursione, con il motivo se non e' mostrato
  char msg[160];
  auto log = [&](const char* fmt, const char* a = "", const char* c = "", const char* d = "") {
    if (!cfg_.meshLog) return;
    snprintf(msg, sizeof(msg), fmt, a, c, d);
    cfg_.meshLog(msg);
  };
  mc::Frame f;
  if (!mc::decode(b, n, f)) {
    char t[24];
    snprintf(t, sizeof(t), "%u byte, tipo %02X", static_cast<unsigned>(n), n ? b[0] : 0);
    log("frame non valido (%s)", t);
    return;
  }
  char hdr[96];
  const char* comma0 = strchr(f.path, ',');
  snprintf(hdr, sizeof(hdr), "%c da %.*s a %s", f.type == mc::kTypeText ? ':' : (f.type == mc::kTypePos ? '!' : '@'),
           static_cast<int>(comma0 ? comma0 - f.path : strlen(f.path)), f.path, f.dest);
  if (!f.fcsOk) {
    log("%s: scartato, FCS sbagliata", hdr);
    return;
  }
  if (f.type != mc::kTypeText) return;  // posizioni e rapporti dei nodi: troppi per scriverli tutti
  // Diretto a noi: il nostro nominativo-SSID o il solo nominativo (alcuni client scrivono senza SSID)
  const bool forUs = cfg_.mcCall[0] && (equalsNoCase(f.dest, cfg_.mcCall) || (cfg_.callsign[0] && equalsNoCase(f.dest, cfg_.callsign)));
  // Un gruppo tra i destinatari impostati (per vedere le risposte ai propri QRV)
  bool forGroup = false;
  for (size_t g = 0; g < mcGroupN_ && !forGroup; g++) forGroup = strcmp(f.dest, mcGroups_[g]) == 0;
  size_t slot = 0;
  if (meshSeen(f.msgId, &slot)) {
    // Copia gia' vista: se il mittente richiede ancora la conferma (la prima si e' persa) la si ripete, ma non prima di 30 s
    if (forUs && meshSeen_[slot].acked && !ackPending_ && static_cast<int32_t>(nowMs - meshSeen_[slot].ackedAtMs) > 30000) {
      // si ripete solo se la copia e' una ritrasmissione: la ricezione di una copia qualsiasi dopo 30 s lo e'
      const char* open = strrchr(f.payload, '{');
      if (open && open[1]) {
        snprintf(ackNum_, sizeof(ackNum_), "%s", open + 1);
        const char* comma = strchr(f.path, ',');
        const size_t sl = comma ? static_cast<size_t>(comma - f.path) : strlen(f.path);
        snprintf(ackDest_, sizeof(ackDest_), "%.*s", static_cast<int>(sl < 15 ? sl : 15), f.path);
        ackPending_ = true;
        ackTries_ = 0;
        ackAtMs_ = nowMs + 1500;
        meshSeen_[slot].ackedAtMs = nowMs;
      }
    }
    log("%s: copia gia' vista", hdr);
    return;
  }
  meshRemember(f.msgId, nowMs);
  slot = (meshNext_ + kMeshSeenSlots - 1) % kMeshSeenSlots;
  if (f.payload[0] == '{') {
    log("%s: messaggio di servizio", hdr);
    return;
  }
  if (!forUs && !forGroup) {
    log("%s: scartato, non e' per noi (%s)", hdr, cfg_.mcCall);
    return;
  }

  const size_t cl = strlen(cfg_.mcCall);
  if (forUs && strncmp(f.payload, cfg_.mcCall, cl) == 0 && f.payload[cl] == ':' && strncmp(f.payload + cl + 1, "ack", 3) == 0) {
    log("%s: conferma di un nostro testo", hdr);
    return;
  }

  // mittente: il primo del percorso
  char src[Inbox::kMaxFrom + 1];
  const char* comma = strchr(f.path, ',');
  const size_t sl = comma ? static_cast<size_t>(comma - f.path) : strlen(f.path);
  snprintf(src, sizeof(src), "%.*s", static_cast<int>(sl < Inbox::kMaxFrom ? sl : Inbox::kMaxFrom), f.path);

  // testo: senza "{NNN" finale, solo ASCII stampabile senza graffe (il resto diventa '?'), al massimo kMaxTextChars; per un
  // gruppo si premette "[gruppo] "
  char text[kMaxTextChars + 1];
  size_t tl = strlen(f.payload);
  char num[8] = {0};
  const char* open = strrchr(f.payload, '{');
  if (open && open[1]) {
    size_t d = 0;
    while (open[1 + d] >= '0' && open[1 + d] <= '9' && d < 5) d++;
    if (d > 0 && open[1 + d] == 0) {
      memcpy(num, open + 1, d);
      tl = static_cast<size_t>(open - f.payload);
    }
  }
  size_t k = 0;
  if (!forUs) {  // gruppo
    k = static_cast<size_t>(snprintf(text, sizeof(text), "[%s] ", f.dest));
    if (k >= kMaxTextChars) k = 0;
  }
  for (size_t i = 0; i < tl && k < kMaxTextChars; i++) {
    const unsigned char c = static_cast<unsigned char>(f.payload[i]);
    text[k++] = (c < 0x20 || c > 0x7E || c == '{' || c == '}') ? '?' : static_cast<char>(c);
  }
  while (k > 0 && text[k - 1] == ' ') k--;
  text[k] = 0;
  if (k == 0 || !inbox_.setText(src, text, k)) {
    log("%s: scartato, testo vuoto", hdr);
    return;
  }
  playing_ = false;
  textOpen_ = false;
  log("%s: MOSTRATO", hdr);
  notify_.messageReceived(true, src);

  if (forUs && num[0] && !ackPending_) {  // richiesta di conferma (solo per i messaggi diretti a noi)
    snprintf(ackNum_, sizeof(ackNum_), "%s", num);
    snprintf(ackDest_, sizeof(ackDest_), "%s", src);
    ackPending_ = true;
    ackTries_ = 0;
    ackAtMs_ = nowMs + 1500;  // un attimo dopo, come i nodi: la copia del mittente e' ancora in giro
    meshSeen_[slot].acked = true;
    meshSeen_[slot].ackedAtMs = nowMs;
  }
}

void VoiceNode::setMeshcomGroups(const char* const* groups, size_t n) {
  mcGroupN_ = 0;
  for (size_t i = 0; i < n && mcGroupN_ < kMcGroupSlots; i++) {
    const char* g = groups[i];
    if (!g || !g[0] || strlen(g) > 6) continue;
    bool digits = true;
    for (size_t k = 0; g[k]; k++) digits = digits && g[k] >= '0' && g[k] <= '9';
    if (!digits) continue;  // solo i gruppi (numeri): "*" e i nominativi non sono gruppi
    snprintf(mcGroups_[mcGroupN_++], sizeof(mcGroups_[0]), "%s", g);
  }
}

// Conferma di ricezione in attesa: si trasmette appena il canale e' libero (al massimo 6 tentativi).
void VoiceNode::meshAckTick(uint32_t nowMs) {
  if (!ackPending_ || state_ != State::Idle || static_cast<int32_t>(nowMs - ackAtMs_) < 0) return;
  if (radio_.channelBusy()) {
    if (++ackTries_ >= 6) ackPending_ = false;
    ackAtMs_ = nowMs + 700;
    return;
  }
  ackPending_ = false;
  mc::Frame f;
  f.type = mc::kTypeText;
  f.msgId = makeMsgId();
  f.flags = mc::kFlagMesh;
  f.maxHop = 4;
  snprintf(f.path, sizeof(f.path), "%s", cfg_.mcCall);
  snprintf(f.dest, sizeof(f.dest), "%s", ackDest_);
  snprintf(f.payload, sizeof(f.payload), "%s:ack%s", ackDest_, ackNum_);
  f.hw = cfg_.mcHw;
  uint8_t wire[kMaxRxBytes];
  const size_t len = mc::encode(f, wire, sizeof(wire));
  if (!len) return;
  meshRemember(f.msgId, nowMs);  // la copia ripetuta di una nostra conferma non e' un messaggio nuovo
  radio_.send(wire, len);
  radio_.startReceive();
}

// Testo verso il destinatario scelto (gruppo, nominativo-SSID o "*"): un solo invio, senza richiesta di conferma. Le
// copie ripetute dai nodi tornano con lo stesso ID e si scartano.
uint32_t VoiceNode::meshcomRemainingMs(uint32_t nowMs) const {
  if (!guest_) return 0;
  const int32_t left = static_cast<int32_t>(guestUntilMs_ - nowMs);
  return left > 0 ? static_cast<uint32_t>(left) : 0;
}

bool VoiceNode::startMeshcom(const char* text, const char* dest, uint32_t nowMs, uint32_t windowMs) {
  if (text && !isValidText(text, strlen(text))) return false;  // vuoto, non ASCII o con le graffe (le graffe sono campi di servizio)
  if (isPlaceholderCallsign(cfg_.callsign) || !cfg_.mcCall[0]) {
    notify_.pttDenied(DenyReason::NoCallsign);
    return false;
  }
  const bool wasGuest = guest_;
  if (wasGuest && state_ == State::Transmitting) {  // gia' sulla rete e sta ancora trasmettendo: si aspetta
    notify_.pttDenied(DenyReason::Transmitting);
    return false;
  }
  if (!guest_) {
    if (state_ == State::Transmitting || recPhase_ != RecPhase::None || pendingStart_) {
      notify_.pttDenied(DenyReason::Transmitting);
      return false;
    }
    if (state_ == State::Receiving) {
      notify_.pttDenied(DenyReason::Receiving);
      return false;
    }
    if (!radio_.setMeshcom(true)) return false;  // la radio non e' passata ai parametri della rete: resta com'era
    guest_ = true;
    ackPending_ = false;
    playing_ = false;
    textOpen_ = false;
    stopMic();
    notify_.meshcomChanged(true);
  }
  guestUntilMs_ = nowMs + windowMs;
  if (text && !sendMeshText(text, dest, nowMs)) {
    if (!wasGuest) endMeshcom();  // invio rifiutato (canale occupato, nessun destinatario...): non si resta sulla rete
    return false;
  }
  return true;
}

void VoiceNode::endMeshcom() {
  if (!guest_) return;
  guest_ = false;
  ackPending_ = false;
  radio_.setMeshcom(false);
  radio_.startReceive();
  notify_.meshcomChanged(false);
}

bool VoiceNode::sendMeshText(const char* text, const char* dest, uint32_t nowMs) {
  if (isPlaceholderCallsign(cfg_.callsign) || !cfg_.mcCall[0]) {
    notify_.pttDenied(DenyReason::NoCallsign);
    return false;
  }
  if (!dest || !dest[0]) {
    notify_.pttDenied(DenyReason::NoDestination);
    return false;
  }
  if (state_ == State::Transmitting) {
    notify_.pttDenied(DenyReason::Transmitting);
    return false;
  }
  if (radio_.channelBusy()) {
    notify_.pttDenied(DenyReason::ChannelBusy);
    return false;
  }
  mc::Frame f;
  f.type = mc::kTypeText;
  f.msgId = makeMsgId();
  f.flags = mc::kFlagMesh;
  f.maxHop = 4;
  snprintf(f.path, sizeof(f.path), "%s", cfg_.mcCall);
  snprintf(f.dest, sizeof(f.dest), "%s", dest);
  snprintf(f.payload, sizeof(f.payload), "%s", text);
  f.hw = cfg_.mcHw;
  textWireLen_ = mc::encode(f, textWire_, sizeof(textWire_));
  if (!textWireLen_) return false;
  meshRemember(f.msgId, nowMs);
  playing_ = false;
  textOpen_ = false;
  textTx_ = true;
  textCopies_ = 1;
  justSent_ = false;
  nextSendMs_ = nowMs;
  state_ = State::Transmitting;
  notify_.txStarted();
  return true;
}

// Avanzamento della ricezione di un messaggio con lunghezza dichiarata: in base al piu' alto numero di sequenza visto,
// cosi' non torna indietro se un pacchetto si perde.
void VoiceNode::reportRxProgress() {
  if (rxIsTest_) return;
  const size_t total = rx_.totalPackets();
  if (total == 0) return;
  // Il primo pacchetto (START) vale 1%; poi in base al piu' alto numero di sequenza visto.
  size_t pct = rx_.highestSeq() == 0 ? 1 : (rx_.highestSeq() + 1) * 100 / total;
  if (pct < 1) pct = 1;
  if (pct > 100) pct = 100;
  if (static_cast<int>(pct) > rxPercent_) {
    rxPercent_ = static_cast<int>(pct);
    notify_.rxProgress(rxPercent_);
  }
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
  memcpy(p.callsign, cfg_.callsign, strnlen(cfg_.callsign, kMaxCallsign));
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
