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
#include "protocol.h"
#include "rx_stream.h"
#include "tx_session.h"

namespace lcr {

struct NodeConfig {
  char callsign[kMaxCallsign + 1] = {0};
  bool fec = true;  // parita' XOR 1 su 4
  // Generatore di ID di sessione (uno per PTT). Se nullptr usa un contatore.
  uint16_t (*newSessionId)() = nullptr;
};

class VoiceNode {
 public:
  enum class State { Idle, Receiving, Transmitting };

  VoiceNode(const NodeConfig& cfg, IRadio& radio, ICodec& codec, IAudioIn& in, IAudioOut& out, INotifier& notify);

  void pttPressed(uint32_t nowMs);
  void pttReleased(uint32_t nowMs);
  // Da chiamare spesso (almeno ogni ~10 ms).
  void tick(uint32_t nowMs);

  // Prova di collegamento (non persistente): con la prova attiva il PTT trasmette frame di prova
  // invece della voce, e dopo aver ricevuto una prova la scheda risponde con il risultato.
  void setTestMode(bool on) { testMode_ = on; }
  bool testMode() const { return testMode_; }

  State state() const { return state_; }
  const RxStream& rx() const { return rx_; }

 private:
  void startTx(uint32_t nowMs);
  void finishCapture(bool limit);
  void txTick(uint32_t nowMs);
  void rxTick(uint32_t nowMs);
  void finishTestRx(uint32_t nowMs);
  void sendTestReply(uint32_t nowMs);
  uint16_t makeSessionId();

  NodeConfig cfg_;
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
};

}  // namespace lcr
