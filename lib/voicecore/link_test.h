// Prova di collegamento: sequenza di frame nota a entrambe le stazioni.
//
// Il trasmettitore invia, con gli stessi pacchetti e la stessa parita' della voce, frame che non sono
// audio ma una sequenza pseudocasuale calcolata da (sessione, numero del frame). Il ricevitore la
// rigenera e confronta bit per bit: sa quindi con certezza quanti frame sono arrivati intatti, quanti
// persi e quanti danneggiati, senza bisogno di microfono, altoparlante o codec.
// Risponde alla domanda "radio e protocollo funzionano?", indipendentemente dall'audio.
//
// Indipendente dalla scheda e testata sul PC (tests/test_link.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.h"

namespace lcr {

// Una prova di collegamento (un passo della prova di potenza): 75 frame = 3 s.
constexpr uint32_t kSweepFrames = 75;

// Frame numero 'index' della prova nella sessione 'session' (kFrameBytes byte).
void testFramePattern(uint16_t session, uint32_t index, uint8_t* out);

struct TestReport {
  uint16_t framesOut = 0;    // frame consegnati dal flusso (buoni + persi + danneggiati)
  uint16_t framesOk = 0;     // identici al previsto
  uint16_t framesBad = 0;    // arrivati ma diversi dal previsto
  uint16_t framesLost = 0;   // non arrivati e non recuperabili
  uint8_t recovered = 0;     // pacchetti ricostruiti con la parita'
  int rssiDbm = 0;           // media degli RSSI dei pacchetti
  int snrDb = 0;             // media degli SNR (0 se la radio non lo fornisce)
  bool snrValid = false;

  // Percentuale di frame ok sui frame consegnati (0 se non ne e' arrivato nessuno), 0-100.
  // Una prova interrotta dal PTT dopo pochi frame perfetti vale quindi 100%, ma non e' "completa".
  int percentOk() const;
  // Prova arrivata fino in fondo (kSweepFrames frame). Se manca la coda non si sa se il PTT e' stato
  // rilasciato prima o se sono andati persi gli ultimi pacchetti: per questo si segnala "parziale".
  bool complete() const { return framesOut >= kSweepFrames; }
  // Nessun frame perso o danneggiato tra quelli consegnati.
  bool clean() const { return framesOut > 0 && framesBad == 0 && framesLost == 0; }
  // "Collegamento buono": prova completa e pulita.
  bool perfect() const { return complete() && clean(); }
};

// Il risultato viaggia in un pacchetto kModeTestReport con 12 byte di payload (2 "frame").
constexpr size_t kTestReportBytes = 12;
void encodeTestReport(const TestReport& r, uint8_t* out);
bool decodeTestReport(const uint8_t* in, size_t len, TestReport& r);

// Accumula l'esito frame per frame mentre il flusso consegna i frame in ordine.
class LinkTestRx {
 public:
  void begin(uint16_t session);
  void onFrame(const uint8_t* frame);  // frame consegnato dal flusso
  void onLost();                       // frame perso e non recuperabile
  void onPacket(int rssiDbm, float snrDb, bool snrValid);
  TestReport report(uint8_t recoveredPackets) const;
  uint16_t session() const { return session_; }

 private:
  uint16_t session_ = 0;
  uint32_t index_ = 0;
  uint16_t ok_ = 0, bad_ = 0, lost_ = 0;
  long rssiSum_ = 0, snrSum_ = 0;
  uint32_t packets_ = 0, snrPackets_ = 0;
};

// Prova di potenza: la stessa prova di collegamento ripetuta a potenza decrescente, dalla massima alla minima, per trovare la potenza
// minima con cui le due schede si sentono. Ogni passo e' una prova breve (kSweepFrames frame); l'altra scheda risponde con l'esito (a
// potenza normale: e' la sua, non quella in prova). Un passo senza risposta vale 0%. Si ferma da solo dopo due passi di fila senza risposta.
constexpr uint32_t kSweepGapMs = 800;      // pausa fra la risposta e il passo successivo
constexpr uint32_t kSweepWaitExtraMs = 3000;  // attesa della risposta dopo l'ultimo pacchetto, oltre al tempo di chiusura della ricezione

class PowerSweep {
 public:
  static constexpr size_t kMaxSteps = 24;
  struct Step {
    int8_t dbm = 0;
    bool answered = false;  // l'altra scheda ha risposto
    uint8_t percent = 0;    // frame arrivati intatti sui frame inviati (0 senza risposta)
    int8_t rssiDbm = 0;
    int8_t snrDb = 0;
    bool snrValid = false;
  };

  void begin(int maxDbm, int minDbm, int restoreDbm, uint32_t frames = kSweepFrames);
  bool active() const { return active_; }
  int currentDbm() const { return cur_; }
  int restoreDbm() const { return restore_; }
  uint32_t frames() const { return frames_; }
  // Registra l'esito del passo in corso (answered = false: nessuna risposta) e passa al successivo. false: la prova e' finita.
  bool record(bool answered, const TestReport& r);
  void stop() { active_ = false; }
  size_t steps() const { return n_; }
  const Step& step(size_t i) const { return steps_[i]; }
  // Potenza minima a cui tutti i frame arrivano intatti, a partire dalla massima senza interruzioni (-1: nemmeno alla massima).
  int minPerfectDbm() const;
  // Potenza minima a cui l'altra scheda ha ancora risposto con almeno un frame (-1: mai).
  int minAnsweredDbm() const;

 private:
  Step steps_[kMaxSteps];
  size_t n_ = 0;
  int cur_ = 0, min_ = 0, restore_ = 0;
  uint32_t frames_ = kSweepFrames;
  bool active_ = false;
};

// Esito della prova di potenza visto da chi ha trasmesso, mandato all'altra scheda perche' lo mostri anche lei (pacchetto kModeTestReport
// con il primo byte >= 0x80: la risposta normale di 12 byte comincia sempre con 0 o 1). Un byte per passo: percentuale di frame intatti, 255 = nessuna risposta.
constexpr uint8_t kSweepNone = 255;
struct SweepSummary {
  uint8_t n = 0;
  int8_t firstDbm = 0;
  int8_t minPerfectDbm = -1;   // -1: nessuna potenza
  int8_t minAnsweredDbm = -1;  // -1: mai
  uint8_t percent[PowerSweep::kMaxSteps] = {0};
};
constexpr size_t kSweepSummaryHeader = 4;
// Il payload e' un multiplo di kFrameBytes (come ogni pacchetto di prova, vedi decodePacket): in coda byte a zero.
inline size_t sweepSummaryBytes(size_t steps) { return ((kSweepSummaryHeader + steps + kFrameBytes - 1) / kFrameBytes) * kFrameBytes; }
size_t encodeSweepSummary(const PowerSweep& w, uint8_t* out, size_t cap);  // 0 se non entra
bool decodeSweepSummary(const uint8_t* in, size_t len, SweepSummary& s);

}  // namespace lcr
