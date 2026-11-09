// Lettura del GPS: interprete delle frasi NMEA (GGA e RMC) e scelta della posizione da usare (GPS o posizione fissa).
// Indipendente dalla scheda e testato sul PC (tests/test_nmea.cpp). La seriale e l'alimentazione del GPS sono in src/gps.cpp.
//
// Il GPS (u-blox sulla T-Beam) manda per default a 9600 baud, una volta al secondo, frasi come
//   $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
//   $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
// L'interprete le legge un carattere alla volta, controlla il checksum e ignora tutto il resto.
#pragma once

#include <cstddef>
#include <cstdint>

#include "position.h"

namespace lcr {

struct GpsFix {
  bool valid = false;  // posizione valida (GGA con qualita' > 0, RMC con stato A)
  int32_t latE7 = 0;   // decimilionesimi di grado
  int32_t lonE7 = 0;
  uint8_t sats = 0;    // satelliti in uso (dalla frase GGA)
};

class NmeaParser {
 public:
  // Un carattere dalla seriale. Ritorna true quando una frase GGA o RMC valida ha appena aggiornato fix().
  bool feed(char c);

  const GpsFix& fix() const { return fix_; }
  uint32_t sentences() const { return sentences_; }        // frasi con checksum corretto (di qualsiasi tipo)
  uint32_t checksumErrors() const { return badChecksum_; }  // frasi scartate per checksum errato o formato non valido

 private:
  bool handle(char* body);  // body senza '$', '*' e checksum

  static constexpr size_t kMaxSentence = 100;
  char buf_[kMaxSentence + 1];
  size_t len_ = 0;
  bool inSentence_ = false;
  bool overflow_ = false;
  GpsFix fix_;
  uint32_t sentences_ = 0;
  uint32_t badChecksum_ = 0;
};

// Una posizione del GPS piu' vecchia di questo non si usa piu' (il GPS manda una posizione al secondo).
constexpr uint32_t kGpsMaxAgeMs = 15000;

// Posizione da usare ora: quella del GPS se e' valida e recente, altrimenti la posizione fissa (QTH), altrimenti nessuna.
// fixAgeMs: millisecondi dall'ultimo aggiornamento valido del GPS.
Position choosePosition(const GpsFix& fix, uint32_t fixAgeMs, const Position& qth);

}  // namespace lcr
