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
  // Escursione sulla rete di testo: parametri MeshCom (profile.h, meshcomProfile) e frequenza delle impostazioni (freqMcKhz), oppure
  // di nuovo quelli del profilo in uso. Se non riesce riporta la radio al profilo in uso e ritorna false.
  bool setMeshcom(bool on) override;
  // Parametri di un profilo qualsiasi alla frequenza data (il ripetitore alterna voce e messaggi), poi di nuovo in ascolto.
  bool setParams(const lcr::Profile& prof, uint32_t freqKhz) override;

  float lastRssi() const { return lastRssi_; }
  bool hasSnr() const override { return true; }
  float lastSnr() const override { return lastSnr_; }
  int lastError() const { return lastError_; }

  // Diagnostica della ricezione, azzerata da resetRxStats(): quanto tempo passa tra l'arrivo di un
  // pacchetto (interrupt della radio) e la sua lettura nel ciclo principale. Se supera la pausa
  // tra i pacchetti del trasmettitore, il pacchetto successivo puo' sovrascrivere quello non ancora letto.
  uint32_t rxPackets() const { return rxPackets_; }
  uint32_t rxCrcErrors() const { return rxCrcErrors_; }
  uint32_t rxMaxDelayMs() const { return rxMaxDelayUs_ / 1000; }
  uint32_t rxOverwritten() const { return rxOverwritten_; }  // interrupt arrivati prima di aver letto il precedente
  void resetRxStats() {
    rxPackets_ = rxCrcErrors_ = rxMaxDelayUs_ = rxOverwritten_ = 0;
  }

 private:
  static void IRAM_ATTR onPacketIsr();
  static volatile bool rxFlag_;
  static volatile bool txActive_;
  static volatile uint32_t rxIsrUs_;
  static volatile uint32_t rxIsrCount_;

  bool applyParams(const lcr::Profile& prof, float mhz);
  SX1278* radio_ = nullptr;
  lcr::Settings settings_;  // impostazioni con cui e' partita la radio (profilo in uso e frequenze)
  float lastRssi_ = 0;
  float lastSnr_ = 0;
  int lastError_ = 0;
  uint32_t rxPackets_ = 0;
  uint32_t rxCrcErrors_ = 0;
  uint32_t rxMaxDelayUs_ = 0;
  uint32_t rxOverwritten_ = 0;
};
