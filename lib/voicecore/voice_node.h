// Macchina a stati del dispositivo.
//
//   Idle  --PTT (canale libero)--> Transmitting --(pacchetti inviati)--> Idle
//   Idle  --pacchetto valido-----> Receiving    --(fine/timeout)------> Idle
//
// Il PTT e' rifiutato se si sta ricevendo, se il canale e' occupato o se la
// trasmissione precedente non e' ancora stata inviata del tutto.
// Non esiste una fase di "bianco": i 5-10 s tra i turni sono solo l'attesa
// dell'operatore.
#pragma once

#include <cstdint>

#include "interfaces.h"
#include "link_test.h"
#include "profile.h"
#include "protocol.h"
#include "rx_stream.h"
#include "tx_session.h"

namespace lcr {

struct NodeConfig {
  char callsign[kMaxCallsign + 1] = {0};
  bool fec = true;  // parita' XOR 1 su 4
  // Generatore di ID di sessione (uno per PTT). Se nullptr usa un contatore.
  uint16_t (*newSessionId)() = nullptr;
  // Profilo di trasmissione (profile.h): Standard o Messaggio.
  uint8_t profile = kProfileStandard;
  // Solo per i test: tempo aggiunto alla pausa tra i pacchetti per simulare il tempo in aria (la radio finta e' istantanea).
  uint32_t extraGapMs = 0;
};

class VoiceNode {
 public:
  enum class State { Idle, Receiving, Transmitting };

  VoiceNode(const NodeConfig& cfg, IRadio& radio, ICodec& codec, IAudioIn& in, IAudioOut& out, INotifier& notify);

  // PTT: una pressione breve (meno di kPttClickMaxMs) e' un clic; tre clic veloci attivano o disattivano la
  // prova di collegamento. La trasmissione parte se il PTT resta premuto, con il microfono gia' bufferizzato.
  void pttPressed(uint32_t nowMs);
  void pttReleased(uint32_t nowMs);
  // Da chiamare spesso (almeno ogni ~10 ms).
  void tick(uint32_t nowMs);

  // Prova di collegamento (non persistente): con la prova attiva il PTT trasmette frame di prova
  // invece della voce, e dopo aver ricevuto una prova la scheda risponde con il risultato.
  // Nel profilo Messaggio la prova di collegamento non c'e' (12 s di frame a SF12 sarebbero circa 17 minuti in aria).
  void setTestMode(bool on) { testMode_ = on && !prof_.message; }
  bool testMode() const { return testMode_; }

  // Modo "registra e poi trasmetti" (prova, non persistente): il PTT registra il messaggio senza trasmettere; dopo il
  // rilascio e un secondo di attesa la scheda lo trasmette. Serve a separare, nelle prove, la registrazione dalla
  // radio (nessuna elaborazione audio mentre la radio trasmette). Ignorato con la prova di collegamento attiva.
  enum class RecPhase { None, Recording, Waiting, Sending };
  // Nel profilo Messaggio il modo e' sempre acceso (non si puo' spegnere).
  void setRecordMode(bool on) { recMode_ = on || prof_.message; }
  bool recordMode() const { return recMode_; }
  RecPhase recPhase() const { return recPhase_; }
  uint32_t recFrames() const { return recCount_; }

  State state() const { return state_; }
  const RxStream& rx() const { return rx_; }

 private:
  void startTx(uint32_t nowMs);
  void beginPtt(uint32_t nowMs);
  void stopMic();
  void finishCapture(bool limit);
  void txTick(uint32_t nowMs);
  void recTick(uint32_t nowMs);
  void finishRecording(uint32_t nowMs, bool limit);
  void setRecPhase(RecPhase p);
  void rxTick(uint32_t nowMs);
  void finishTestRx(uint32_t nowMs);
  void sendTestReply(uint32_t nowMs);
  uint16_t makeSessionId();

  NodeConfig cfg_;
  const Profile& prof_;
  uint32_t maxTurnFrames_;
  IRadio& radio_;
  ICodec& codec_;
  IAudioIn& in_;
  IAudioOut& out_;
  INotifier& notify_;

  State state_ = State::Idle;
  TxSession tx_;
  RxStream rx_;

  bool releasing_ = false;
  bool limitHit_ = false;
  uint32_t framesCaptured_ = 0;
  uint16_t fallbackSession_ = 1;

  uint32_t nextPlayMs_ = 0;
  uint32_t nextSendMs_ = 0;  // non trasmettere prima di questo istante (pausa tra pacchetti)
  bool justSent_ = false;
  bool callsignNotified_ = false;

  // PTT: clic e attesa prima di trasmettere
  bool pendingStart_ = false;  // PTT premuto, in attesa di capire se e' un clic o l'inizio di una trasmissione
  uint32_t pressMs_ = 0;
  uint8_t clickCount_ = 0;
  uint32_t lastClickMs_ = 0;
  bool micStarted_ = false;

  // Registra e poi trasmetti
  bool recMode_ = false;
  RecPhase recPhase_ = RecPhase::None;
  uint8_t recBuf_[kMaxTurnFrames * kFrameBytes];
  uint32_t recCount_ = 0;
  uint32_t recSent_ = 0;
  uint32_t recReleaseMs_ = 0;

  // Prova di collegamento
  bool testMode_ = false;
  bool txTest_ = false;             // la trasmissione in corso e' una prova
  uint32_t testFramesSent_ = 0;
  uint32_t nextTestFrameMs_ = 0;
  uint16_t lastTestSession_ = 0;    // ultima prova trasmessa da qui (per riconoscere la risposta)
  bool lastTestValid_ = false;
  bool rxIsTest_ = false;           // la ricezione in corso e' una prova
  LinkTestRx linkRx_;
  bool replyPending_ = false;
  uint32_t replyAtMs_ = 0;
  uint8_t replyTries_ = 0;
  uint16_t replySession_ = 0;
  TestReport replyReport_;
  uint16_t notifiedSession_ = 0;
  bool playbackNotified_ = false;  // rxPlaybackStarted() gia' segnalato in questa ricezione
};

}  // namespace lcr
