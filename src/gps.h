// GPS della T-Beam V1.2 (u-blox, 9600 baud, frasi NMEA una volta al secondo).
// Solo per ESP32 (Arduino): non e' compilato sul PC. L'interprete NMEA e la scelta della posizione sono in
// lib/voicecore/nmea.h (testati sul PC). Sulla LoRa32 T3 non c'e' il GPS e tutte le funzioni sono neutre.
#pragma once

#include <Arduino.h>

#include "nmea.h"
#include "pmu.h"

class Gps {
 public:
  // Accende il GPS (alimentazione dal PMU) e apre la seriale. Solo se 'enabled' e la scheda ha il GPS.
  void begin(bool enabled, Pmu& pmu);
  // Da chiamare spesso: legge i caratteri disponibili e scrive in seriale lo stato del GPS.
  void loop(uint32_t nowMs);

  bool active() const { return active_; }               // GPS acceso e seriale aperta
  bool hasFix(uint32_t nowMs) const;                    // posizione valida e recente
  uint8_t satellites() const { return parser_.fix().sats; }
  // Posizione da usare ora: il GPS se ha un fix recente, altrimenti 'qth' (posizione fissa), altrimenti nessuna.
  lcr::Position position(uint32_t nowMs, const lcr::Position& qth) const;

 private:
  bool active_ = false;
  lcr::NmeaParser parser_;
  uint32_t lastFixMs_ = 0;      // ultimo aggiornamento con posizione valida
  bool everFixed_ = false;
  bool lastHadFix_ = false;
  uint32_t lastLogMs_ = 0;
  uint32_t charsRead_ = 0;
};
