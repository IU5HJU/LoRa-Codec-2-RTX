// Adattatore radio: SX1278 (433 MHz) con RadioLib, implementa lcr::IRadio.
// Solo per ESP32 (Arduino): non e' compilato nei test sul PC.
#pragma once

#include <Arduino.h>
#include <RadioLib.h>

#include "interfaces.h"
#include "settings.h"

class RadioLora : public lcr::IRadio {
 public:
  // Inizializza SPI e radio con frequenza e potenza delle impostazioni, il resto da user_config.h. ATTENZIONE:
  // collegare l'antenna prima di chiamarla (il modulo puo' trasmettere).
  bool begin(const lcr::Settings& settings);

  bool channelBusy() override;
  bool send(const uint8_t* data, size_t len) override;
  void startReceive() override;
  size_t receive(uint8_t* buf, size_t cap, int* rssi) override;

  float lastRssi() const { return lastRssi_; }
  float lastSnr() const { return lastSnr_; }
  int lastError() const { return lastError_; }

 private:
  static void IRAM_ATTR onPacketIsr();
  static volatile bool rxFlag_;
  static volatile bool txActive_;

  SX1278* radio_ = nullptr;
  float lastRssi_ = 0;
  float lastSnr_ = 0;
  int lastError_ = 0;
};
