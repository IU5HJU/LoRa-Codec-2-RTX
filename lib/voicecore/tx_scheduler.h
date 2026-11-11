// Regista della scheda TX del ripetitore (docs/studio-ripetitore.md, "Estensione: ripetere i messaggi").
//
// Una sola radio, la frequenza di uscita, due profili: la voce (quello del ripetitore) e il Messaggio (SF12/BW125). Il regista
// decide a ogni giro cosa fa la radio:
//   Voice   parametri della voce: ritrasmette i pacchetti che arrivano dalla scheda RX;
//   Listen  parametri del Messaggio, in ascolto: i messaggi che le stazioni mandano all'uscita finiscono nell'archivio;
//   Relay   parametri del Messaggio, in trasmissione: ritrasmette un messaggio dell'archivio (sessione nuova, flag RELAYED).
// Regole (decise l'11/11/2026):
//   - la voce ha sempre la precedenza: se arriva mentre un messaggio e' in ritrasmissione, il messaggio si ferma tra un pacchetto
//     e l'altro (un pacchetto in corso non si interrompe: a SF12 sono circa 3,4 s) e ripartira' dall'inizio;
//   - dopo l'ultimo pacchetto di voce la radio resta sui parametri della voce per kHoldMs, poi passa all'ascolto messaggi;
//   - un messaggio si ritrasmette solo dopo kTailMs di silenzio sull'uscita (non si entra nelle pause di un QSO);
//   - prima di ogni pacchetto del messaggio si controlla che il canale sia libero (CAD); occupato oltre kBusyAbortMs il
//     messaggio si interrompe e ripartira' dall'inizio;
//   - tra un messaggio e il successivo, e dopo l'ultimo pacchetto ricevuto in ascolto, passano kBetweenMsgMs.
// Qui c'e' solo la logica, senza orologio ne' pause (si passa 'now'): provata sul PC (tests/test_tx_scheduler.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces.h"
#include "message_store.h"
#include "profile.h"
#include "repeater_link.h"

namespace lcr {

class TxScheduler {
 public:
  static constexpr uint32_t kHoldMs = 3000;
  static constexpr uint32_t kTailMs = 20000;
  static constexpr uint32_t kBusyAbortMs = 5000;
  static constexpr uint32_t kBusyRetryMs = 300;
  static constexpr uint32_t kBetweenMsgMs = 5000;
  static constexpr uint32_t kParamRetryMs = 1000;

  enum class State : uint8_t { Voice, Listen, Relay };

  struct Stats {
    uint32_t voiceSent = 0;
    uint32_t relayed = 0;        // messaggi ritrasmessi per intero
    uint32_t relayPackets = 0;   // pacchetti di messaggi trasmessi (anche di tentativi interrotti)
    uint32_t interrupted = 0;    // ritrasmissioni fermate dalla voce
    uint32_t busyAborts = 0;     // ritrasmissioni fermate dal canale occupato
    uint32_t paramErrors = 0;
    uint32_t messageRx = 0;      // pacchetti arrivati in ascolto
  };

  TxScheduler(IRadio& radio, MessageStore& store, const Profile& voice, const Profile& message, uint32_t freqKhz, uint16_t sessionSeed);

  // Un pacchetto dalla scheda RX (formato del filo). false se la coda e' piena: il pacchetto si perde (contato dalla coda).
  bool onLinkPacket(const uint8_t* wire, size_t len) { return queue_.push(wire, len); }
  // Un giro: al piu' una trasmissione. mayTransmit false (N0CALL): niente si trasmette, la voce in coda si scarta.
  void tick(uint32_t nowMs, bool mayTransmit);

  // false (profilo Messaggio ripetuto come voce, o messaggi spenti): la radio non va mai in ascolto messaggi ne' ritrasmette.
  void setMessagesEnabled(bool on) { messages_ = on; }
  State state() const { return state_; }
  const TxQueue& queue() const { return queue_; }
  const Stats& stats() const { return stats_; }

 private:
  bool switchTo(State s, uint32_t nowMs);
  void listen(uint32_t nowMs);
  void abortRelay(uint32_t nowMs, bool byVoice);
  void relayStep(uint32_t nowMs);
  bool tailElapsed(uint32_t nowMs) const { return !haveVoice_ || nowMs - lastVoiceMs_ >= kTailMs; }

  IRadio& radio_;
  MessageStore& store_;
  const Profile& voice_;
  const Profile& message_;
  uint32_t freqKhz_;
  TxQueue queue_;
  State state_ = State::Voice;  // la radio parte con i parametri della voce
  bool haveVoice_ = false;
  uint32_t lastVoiceMs_ = 0;
  uint32_t nextRelayMs_ = 0;
  uint32_t nextParamMs_ = 0;
  uint16_t nextSession_;
  uint16_t session_ = 0;
  size_t relayIdx_ = 0;
  bool haveRelayTx_ = false;
  uint32_t lastRelayTxMs_ = 0;
  uint32_t busySinceMs_ = 0;
  bool busy_ = false;
  bool messages_ = true;
  Stats stats_;
};

}  // namespace lcr
