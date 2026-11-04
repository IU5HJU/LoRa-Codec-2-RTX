// Impostazioni dell'operatore nella memoria non volatile (NVS) dell'ESP32.
// Solo per ESP32 (Arduino): non e' compilato nei test sul PC.
#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "settings.h"

// Valori compilati in user_config.h: si usano se la memoria e' vuota o non valida.
lcr::Settings compileTimeSettings();

class NvsSettingsStorage : public lcr::ISettingsStorage {
 public:
  bool load(lcr::Settings& out) override;
  bool save(const lcr::Settings& s) override;
  bool erase() override;
};
