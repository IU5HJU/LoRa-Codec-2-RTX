// Comandi del sysop al ripetitore (modo 5) e controllo del ripetitore (docs/studio-ripetitore.md, "Comandi del sysop").
//
// Solo il sysop puo' comandare: ogni comando e' firmato con HMAC-SHA256 (troncato a 8 byte) con una chiave condivisa di 16 byte
// e porta un contatore che deve superare l'ultimo accettato (una registrazione riascoltata non funziona). La firma NON cifra: il
// comando resta leggibile. Qui c'e' solo la logica, senza radio ne' orologio: provata sul PC (tests/test_sysop.cpp).
//
// Pacchetto (modo 5, START|END|CALLSIGN con il nominativo di chi manda), payload:
//   comando  [codice 1][parametro 2, big-endian][contatore 4, big-endian][firma 8]                    = 15 byte
//   risposta [0x80|codice 1][esito 1][contatore 4][dati 4][firma 8]                                   = 18 byte
// La firma copre tutto il pacchetto (come esce da encodePacket) tranne gli ultimi 8 byte. La risposta e' firmata con la stessa
// chiave e ripete il contatore del comando: il sysop sa a quale comando risponde e che viene davvero dal ripetitore.
#pragma once

#include <cstddef>
#include <cstdint>

#include "packet.h"
#include "protocol.h"

namespace lcr {

constexpr size_t kSysopKeyBytes = 16;
constexpr size_t kSysopSigBytes = 8;

enum SysopCode : uint8_t {
  kSysopStatus = 1,      // risponde con lo stato
  kSysopOff = 2,         // la TX smette di trasmettere finche' non arriva RIACCENDI (la RX ascolta sempre)
  kSysopOn = 3,          // torna in servizio
  kSysopOffMinutes = 4,  // spegnimento a tempo, parametro = minuti (1..kSysopMaxOffMinutes)
  kSysopRestart = 5,     // riavvio delle due schede
  kSysopIdentify = 6,    // trasmette subito il nominativo
  kSysopPower = 7,       // potenza di uscita in dBm, parametro entro i limiti
};
constexpr uint16_t kSysopMaxOffMinutes = 1440;

// Voci del modo Sysop (elenco al posto dei testi predefiniti, sulla scheda e nella pagina web).
struct SysopMenuEntry {
  const char* label;
  uint8_t code;
  uint16_t param;
};
const SysopMenuEntry* sysopMenuEntries(size_t& count);
constexpr uint8_t kSysopReplyBit = 0x80;

enum SysopResult : uint8_t {
  kSysopOk = 0,
  kSysopBadParam = 1,    // parametro fuori limiti
  kSysopUnknown = 2,     // codice sconosciuto
  kSysopNotAllowed = 3,  // non consentito (nessun regista di controllo)
  kSysopCounterLow = 4,  // contatore troppo basso: la risposta ripete l'ultimo contatore accettato, la stazione si risincronizza
};

// Senza valori iniziali nei membri: il firmware e' C++11, dove una struttura cosi' si inizializza con le graffe solo se e' un aggregato.
struct SysopCommand {
  uint8_t code;
  uint16_t param;
  uint32_t counter;
};

struct SysopReply {
  uint8_t code;
  uint8_t result;
  uint32_t counter;
  uint32_t data;
};

Packet makeSysopCommand(uint16_t session, const char* callsign, const uint8_t key[kSysopKeyBytes], const SysopCommand& c);
Packet makeSysopReply(uint16_t session, const char* callsign, const uint8_t key[kSysopKeyBytes], const SysopReply& r);
// Verifica la firma di un pacchetto del modo 5 e legge la risposta (per la stazione del sysop). false se non e' una risposta
// o la firma non torna.
bool parseSysopReply(const Packet& p, const uint8_t key[kSysopKeyBytes], SysopReply& out);

// Lato ripetitore: accetta solo comandi del sysop firmati e con contatore nuovo.
class SysopAuth {
 public:
  // Replay: stesso contatore dell'ultimo (la seconda copia di un comando): silenzio. Stale: firma valida ma contatore piu' basso
  // dell'ultimo: la stazione e' fuori sincronia, il ripetitore glielo dice (rate limit nella TX).
  enum class Verdict : uint8_t { Ok, NotCommand, WrongCallsign, BadSignature, Replay, Stale, NoKey };

  void setKey(const uint8_t key[kSysopKeyBytes]);
  void clearKey() { haveKey_ = false; }
  bool hasKey() const { return haveKey_; }
  void setSysop(const char* callsign);
  void setLastCounter(uint32_t c) { last_ = c; }
  uint32_t lastCounter() const { return last_; }

  // Ok: il comando e' valido e il contatore e' avanzato (da salvare in memoria non volatile).
  Verdict check(const Packet& p, SysopCommand& out);

  uint32_t rejected() const { return rejected_; }

 private:
  uint8_t key_[kSysopKeyBytes] = {0};
  bool haveKey_ = false;
  char sysop_[kMaxCallsign + 1] = {0};
  uint32_t last_ = 0;
  uint32_t rejected_ = 0;
};

// Stato di servizio della scheda TX e sicurezze (limite di una trasmissione continua).
class RepeaterControl {
 public:
  static constexpr uint32_t kMaxVoiceTxMs = 60000;    // trasmissione continua di voce
  static constexpr uint32_t kMaxMessageTxMs = 180000; // trasmissione continua di un messaggio (circa 139 s) 
  static constexpr uint32_t kSafetyOffMs = 300000;    // spegnimento automatico dopo un superamento
  static constexpr uint32_t kContinuousGapMs = 1500;  // pausa che interrompe una trasmissione "continua"

  struct Effects {
    bool restart = false;      // riavviare (dopo aver mandato la risposta)
    bool identify = false;     // trasmettere subito il nominativo
    int power = -1;            // nuova potenza in dBm, -1 = invariata
    bool forceReply = false;   // la risposta va trasmessa anche se la TX e' appena stata spenta da questo comando
  };

  RepeaterControl(int minPowerDbm, int maxPowerDbm, int currentPowerDbm) : minPower_(minPowerDbm), maxPower_(maxPowerDbm), power_(currentPowerDbm) {}

  // Esegue un comando gia' verificato. Ritorna l'esito; 'data' e' il carico della risposta (stato).
  SysopResult apply(const SysopCommand& c, uint32_t nowMs, Effects& fx, uint32_t& data);

  // La TX puo' trasmettere? (spenta dal sysop, a tempo o dalla sicurezza: no)
  bool txAllowed(uint32_t nowMs);
  // Da chiamare a ogni trasmissione: 'airMs' e' la durata in aria. Se la trasmissione continua supera il limite la TX si spegne
  // per kSafetyOffMs. Ritorna false se si e' appena spenta.
  bool noteTx(uint32_t nowMs, uint32_t airMs, bool message);

  bool off() const { return off_; }
  int power() const { return power_; }
  uint32_t safetyTrips() const { return trips_; }
  // Stato sintetico per STATO: byte 3 = 1 se spenta, byte 2 = potenza dBm, byte 1-0 = minuti che restano allo spegnimento a tempo
  // (0 = nessuno o senza scadenza; 0xFFFF se senza scadenza e spenta).
  uint32_t statusData(uint32_t nowMs) const;

 private:
  int minPower_, maxPower_, power_;
  bool off_ = false;
  bool timed_ = false;
  uint32_t offUntilMs_ = 0;
  bool contActive_ = false;
  uint32_t contStartMs_ = 0;
  uint32_t contEndMs_ = 0;
  uint32_t trips_ = 0;
};

}  // namespace lcr
