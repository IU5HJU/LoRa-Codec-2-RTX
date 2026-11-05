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
  // Prova arrivata fino in fondo (kTestFrames frame). Se manca la coda non si sa se il PTT e' stato
  // rilasciato prima o se sono andati persi gli ultimi pacchetti: per questo si segnala "parziale".
  bool complete() const { return framesOut >= kTestFrames; }
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

}  // namespace lcr
