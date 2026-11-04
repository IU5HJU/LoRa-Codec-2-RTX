// Gestione dell'alimentazione.
//
// Sulla T-Beam V1.2 l'AXP2101 alimenta la radio e il GPS: va inizializzato PRIMA della
// radio, altrimenti la radio non e' alimentata. Sulla LoRa32 T3 non c'e' nessun PMU e
// tutte le funzioni sono neutre. Solo per ESP32 (Arduino): non e' compilato sul PC.
#pragma once

#include <Arduino.h>

class Pmu {
 public:
  // Inizializza il PMU della scheda. Ritorna false se la scheda ne ha uno e non risponde.
  bool begin();
  // true solo se un PMU e' stato trovato e inizializzato.
  bool present() const { return present_; }

  // Valori della batteria: -1 se non disponibili (nessun PMU o batteria assente).
  int batteryMillivolt();
  int batteryPercent();
  bool charging();
  bool usbPresent();

 private:
  bool present_ = false;
};
