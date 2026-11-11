// Archivio dei messaggi del ripetitore (docs/studio-ripetitore.md, "Estensione: ripetere i messaggi").
//
// La scheda TX ascolta il profilo Messaggio quando e' libera, riceve un messaggio intero, ricostruisce con la parita' i
// pacchetti persi e lo tiene da parte; lo ritrasmette appena il ripetitore e' libero (la voce ha sempre la precedenza).
// Qui c'e' solo la logica, senza radio ne' orologio, provata sul PC (tests/test_message_store.cpp).
//
// Regole (decise l'11/11/2026):
//   - un solo messaggio in ricezione alla volta; un'altra sessione che arriva mentre si riceve si ignora;
//   - massimo 15 s di voce: lo START dichiara i pacchetti totali e un messaggio piu' lungo si scarta subito, per intero;
//   - un messaggio e' completo quando ci sono tutti i pacchetti dati (quelli persi si ricostruiscono con la parita');
//   - la parita' si rigenera: ogni tratta ha la sua protezione;
//   - ritrasmissione con una sessione NUOVA (la sessione vecchia, per un ricevitore, puo' essere gia' chiusa) e flag RELAYED;
//   - si tengono al massimo 3 messaggi pronti e si scartano quelli piu' vecchi di 30 minuti.
#pragma once

#include <cstddef>
#include <cstdint>

#include "packet.h"
#include "profile.h"
#include "protocol.h"

namespace lcr {

class MessageStore {
 public:
  static constexpr uint32_t kMaxVoiceMs = kMessageMaxTurnMs;  // il TOT del profilo Messaggio: 15 s
  static constexpr size_t kMaxPacketsPerMessage = 48;       // 15 s sono 42 pacchetti (dati e parita'), con margine
  static constexpr size_t kQueue = 3;                       // messaggi pronti
  static constexpr uint32_t kSilenceMs = 12000;             // silenzio dopo cui un messaggio incompleto si abbandona
  static constexpr uint32_t kExpireMs = 30UL * 60UL * 1000UL;

  enum class Result : uint8_t {
    Accepted,   // pacchetto messo da parte, il messaggio non e' ancora completo
    Complete,   // il messaggio e' completo e pronto per la ritrasmissione
    Ignored,    // non e' un pacchetto di messaggio da ripetere (modo, flag "ripetuto", sessione diversa)
    TooLong,    // oltre il limite: il messaggio non si ripete
    Duplicate,  // pacchetto gia' ricevuto
  };

  struct Stats {
    uint32_t completed = 0;
    uint32_t tooLong = 0;
    uint32_t abandoned = 0;   // incompleti per silenzio
    uint32_t recovered = 0;   // pacchetti ricostruiti con la parita'
    uint32_t droppedOld = 0;  // scartati per coda piena o scaduti
  };

  MessageStore();
  void reset();

  Result onPacket(const Packet& p, uint32_t nowMs);
  // Da chiamare spesso: abbandona la ricezione dopo kSilenceMs senza pacchetti ed elimina i messaggi scaduti.
  void tick(uint32_t nowMs);

  bool receiving() const { return cur_.active; }
  size_t readyCount() const { return readyN_; }

  // Il messaggio pronto piu' vecchio (da ritrasmettere per primo). packets = numero di pacchetti.
  size_t readyPackets() const { return readyN_ ? ready_[0].n : 0; }
  // i-esimo pacchetto da ritrasmettere, con la sessione nuova e il flag RELAYED. false se i e' fuori intervallo.
  bool readyPacket(size_t i, uint16_t newSession, Packet& out) const;
  // Tempo in aria (ms) del messaggio pronto piu' vecchio con i parametri radio del profilo (stima di Semtech).
  uint32_t readyAirtimeMs(const Profile& prof) const;
  // Da chiamare dopo l'ultimo pacchetto trasmesso. Se la ritrasmissione si interrompe per la voce non si chiama: il
  // messaggio resta in testa e si riparte dall'inizio.
  void popReady();

  const Stats& stats() const { return stats_; }

 private:
  struct Msg {
    size_t n = 0;
    uint32_t completedMs = 0;
    Packet pk[kMaxPacketsPerMessage];
  };
  struct Cur {
    bool active = false;
    uint16_t session = 0;
    uint32_t lastMs = 0;
    bool fec = true;
    size_t total = 0;  // 0 = non noto
    int endSeq = -1;
    size_t highest = 0;
    bool have[kMaxPacketsPerMessage];
    bool parityDone[kMaxPacketsPerMessage];
    Packet pk[kMaxPacketsPerMessage];
  };

  bool isParitySeq(size_t s) const { return cur_.fec && (s % kParityPeriod) == kParityGroup; }
  void recoverAll();
  bool complete() const;
  void finish(uint32_t nowMs);
  void abort();

  Cur cur_;
  // Ultima sessione completata: i pacchetti che arrivano dopo (la parita' finale) non aprono una ricezione nuova.
  bool doneValid_ = false;
  uint16_t doneSession_ = 0;
  uint32_t doneMs_ = 0;
  Msg ready_[kQueue];
  size_t readyN_ = 0;
  Stats stats_;
};

}  // namespace lcr
