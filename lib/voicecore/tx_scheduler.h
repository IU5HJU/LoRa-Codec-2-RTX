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
//   - prima della ritrasmissione, con i parametri della voce, l'annuncio di occupato (modo 6) con il tempo previsto, due volte; a
//     ritrasmissione conclusa (non interrotta dalla voce) un annuncio con 0 secondi. Solo informativo: la voce passa lo stesso;
//   - tra un messaggio e il successivo, e dopo l'ultimo pacchetto ricevuto in ascolto, passano kBetweenMsgMs.
// Qui c'e' solo la logica, senza orologio ne' pause (si passa 'now'): provata sul PC (tests/test_tx_scheduler.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces.h"
#include "message_store.h"
#include "profile.h"
#include "radio_test.h"
#include "repeater_link.h"
#include "sysop.h"

namespace lcr {

class TxScheduler {
 public:
  static constexpr uint32_t kHoldMs = 3000;
  static constexpr uint32_t kTailMs = 20000;
  static constexpr uint32_t kBusyAbortMs = 5000;
  static constexpr uint32_t kBusyRetryMs = 300;
  static constexpr uint32_t kBetweenMsgMs = 5000;
  static constexpr uint32_t kParamRetryMs = 1000;
  static constexpr uint32_t kStaleReplyGapMs = 10000;
  static constexpr uint32_t kBeaconFirstMs = 15000;  // primo identificativo dopo l'avvio

  // Announce e Release usano i parametri della voce: mandano l'annuncio di occupato prima e dopo un messaggio.
  enum class State : uint8_t { Voice, Listen, Relay, Announce, Release, Morse };

  struct Stats {
    uint32_t voiceSent = 0;
    uint32_t relayed = 0;        // messaggi ritrasmessi per intero
    uint32_t relayPackets = 0;   // pacchetti di messaggi trasmessi (anche di tentativi interrotti)
    uint32_t interrupted = 0;    // ritrasmissioni fermate dalla voce
    uint32_t busyAborts = 0;     // ritrasmissioni fermate dal canale occupato
    uint32_t paramErrors = 0;
    uint32_t messageRx = 0;      // pacchetti arrivati in ascolto
    uint32_t replies = 0;        // risposte ai comandi del sysop trasmesse
    uint32_t repliesDropped = 0; // risposte non trasmesse perche' la TX e' spenta
    uint32_t morseIds = 0;       // identificativi in Morse trasmessi per intero
    uint32_t morseAborted = 0;   // interrotti dalla voce, dalla TX spenta o da un errore della radio
    uint32_t beacons = 0;        // identificativi trasmessi
    uint32_t announces = 0;      // annunci di occupato mandati (copie comprese)
  };

  TxScheduler(IRadio& radio, MessageStore& store, const Profile& voice, const Profile& message, uint32_t freqKhz, uint16_t sessionSeed);

  // Un pacchetto dalla scheda RX (formato del filo). false se la coda e' piena: il pacchetto si perde (contato dalla coda).
  bool onLinkPacket(const uint8_t* wire, size_t len) { return queue_.push(wire, len); }
  // Un giro: al piu' una trasmissione. mayTransmit false (N0CALL): niente si trasmette, la voce in coda si scarta.
  void tick(uint32_t nowMs, bool mayTransmit);

  // Nominativo del ripetitore: se c'e', prima di ritrasmettere un messaggio si manda l'annuncio di occupato (modo 6, protocol.h).
  void setCallsign(const char* callsign);
  // Identificativo periodico del ripetitore (testo con seq = kTextSeqBeacon, packet.h): ogni 'intervalMs' (0 = spento), con il nominativo
  // impostato da setCallsign. Parte a canale libero: mai sopra la voce in coda, una ritrasmissione di messaggio o la ricezione di un
  // messaggio (al massimo aspetta la fine del messaggio in corso); con la TX spenta dal sysop non parte.
  void setBeaconInterval(uint32_t intervalMs) { beaconIntervalMs_ = intervalMs; }
  // Identificativo in Morse con tono in FM stretta (docs/studio-identificativo-cw.md), subito dopo il testo dell'identificativo: il
  // nominativo in Morse a 'wpm' parole al minuto, tono 'toneHz', deviazione 'devKhz'. Chi non ha le schede LoRa lo sente con un
  // ricevitore FM qualsiasi. La voce lo interrompe tra un elemento e l'altro (l'identificativo in testo e' gia' uscito).
  void setMorseId(bool on, int wpm = 15, float toneHz = 700.0f, float devKhz = 2.5f);
  // Comandi del sysop: lo stato di servizio (spento, limiti di trasmissione) e la chiave con cui si firmano le risposte.
  void setControl(RepeaterControl* control, const uint8_t key[kSysopKeyBytes]);
  // Un comando gia' verificato dalla RX: lo esegue e trasmette la risposta firmata (anche se questo comando ha appena spento la TX);
  // 'fx' dice cosa resta da fare alla scheda (potenza, riavvio). L'identificazione la trasmette lo stesso regista.
  SysopResult onControl(const SysopCommand& c, uint32_t nowMs, RepeaterControl::Effects& fx);
  // Comando con firma valida ma contatore troppo basso (la RX ha gia' rifiutato): risponde con l'ultimo contatore accettato, perche' la
  // stazione si risincronizzi. Al massimo una risposta ogni kStaleReplyGapMs (chi riascolta una registrazione non deve far trasmettere).
  void onStaleCommand(uint8_t code, uint32_t lastCounter, uint32_t nowMs);
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
  void endRelayState(uint32_t nowMs);
  void sendNow(const Packet& p, uint32_t nowMs, bool message);
  void sendAndNote(const uint8_t* wire, size_t len, uint32_t nowMs, bool message);
  bool beaconDue(uint32_t nowMs) const;
  void startMorse(uint32_t nowMs);
  void morseStep(uint32_t nowMs);
  void endMorse(bool aborted);
  void announceStep(uint32_t nowMs);
  void releaseStep(uint32_t nowMs);
  bool sendBusy(uint16_t seconds, uint32_t nowMs);
  uint16_t busySecondsForReady() const;
  static bool voiceParams(State s) { return s == State::Voice || s == State::Announce || s == State::Release; }
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
  char callsign_[kMaxCallsign + 1] = {0};
  RepeaterControl* control_ = nullptr;
  uint8_t key_[kSysopKeyBytes] = {0};
  bool morseOn_ = false;
  RadioTestRequest morseReq_;
  MorseBurst morseBursts_[160];
  size_t morseN_ = 0;
  size_t morseIdx_ = 0;
  uint32_t morseNextMs_ = 0;
  float morseKbps_ = 1.4f;
  uint32_t beaconIntervalMs_ = 0;
  bool haveBeacon_ = false;
  uint32_t lastBeaconMs_ = 0;
  bool haveStaleReply_ = false;
  uint32_t lastStaleReplyMs_ = 0;
  uint8_t annSent_ = 0;  // copie dell'annuncio gia' mandate
  uint32_t lastAnnMs_ = 0;
  uint16_t annSession_ = 0;
  uint16_t annSeconds_ = 0;
  Stats stats_;
};

}  // namespace lcr
