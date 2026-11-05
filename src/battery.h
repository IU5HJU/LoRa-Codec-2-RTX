// Tensione della batteria (o della linea di alimentazione) da mostrare sul display.
//
//  - T-Beam V1.2: la legge il chip di gestione AXP2101 (come nel firmware di Guzman:
//    "non collegata" se la batteria e' assente).
//  - LoRa32 T3 V1.6.1: ADC calibrato su GPIO35 con partitore 100k/100k. Con l'USB e senza
//    batteria misura la linea di carica (circa 4 V), non i 5 V dell'USB.
//
// Lettura non bloccante: il ciclo principale non si ferma (a differenza delle 20 letture con
// delay del firmware di Guzman). Solo per ESP32 (Arduino): non e' compilato sul PC.
#pragma once

#include <Arduino.h>

#include "battery_info.h"
#include "pmu.h"

class Battery {
 public:
  void begin(Pmu& pmu);
  // Da chiamare spesso nel loop.
  void update(uint32_t nowMs);
  // Tensione in volt; negativa se non disponibile.
  float volts() const { return volts_; }

 private:
  Pmu* pmu_ = nullptr;
  float volts_ = -1.0f;
  uint32_t lastMs_ = 0;
  lcr::AverageFilter filter_;
  lcr::BatterySmoother smoother_;  // T3: media lenta, il caricatore oscilla
};
