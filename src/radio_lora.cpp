#include "radio_lora.h"

#include <SPI.h>
#include <esp_timer.h>

#include "board_pins.h"
#include "user_config.h"

volatile bool RadioLora::rxFlag_ = false;
volatile bool RadioLora::txActive_ = false;
volatile uint32_t RadioLora::rxIsrUs_ = 0;
volatile uint32_t RadioLora::rxIsrCount_ = 0;

void IRAM_ATTR RadioLora::onPacketIsr() {
  if (!txActive_) {  // durante la TX DIO0 segnala "trasmesso", non "ricevuto"
    rxIsrCount_ = rxIsrCount_ + 1;
    if (!rxFlag_) rxIsrUs_ = static_cast<uint32_t>(esp_timer_get_time());
    rxFlag_ = true;
  }
}

// Cambio di frequenza dentro la banda 430-440 MHz. SX1262: senza ricalibrare l'immagine a ogni cambio (RadioLib lo farebbe per salti di piu'
// di qualche MHz, cioe' a ogni trasmissione con lo shift di 10 MHz): la calibrazione fatta all'avvio vale per tutta la banda 430-440.
int RadioLora::setFreq(float mhz) {
#if defined(RADIO_SX1262)
  return radio_->setFrequency(mhz, true);
#else
  return radio_->setFrequency(mhz);
#endif
}

bool RadioLora::begin(const lcr::Settings& settings) {
  settings_ = settings;
  txShiftKhz_ = settings.role == lcr::kRoleNormal ? settings.shiftKhz : 0;  // lo shift vale solo per la stazione normale
  SPI.begin(pins::kRadioSclk, pins::kRadioMiso, pins::kRadioMosi, pins::kRadioCs);
#if defined(RADIO_SX1262)
  // SX1262: l'interrupt e' DIO1, e c'e' il pin BUSY (RadioLib lo aspetta tra un comando e l'altro)
  module_ = new Module(pins::kRadioCs, pins::kRadioDio1, pins::kRadioRst, pins::kRadioBusy, SPI);
#else
  module_ = new Module(pins::kRadioCs, pins::kRadioDio0, pins::kRadioRst, pins::kRadioDio1, SPI);
#endif
  radio_ = new RadioChip(module_);

  return initLora();
}

// I parametri LoRa vengono dal profilo scelto (profile.h): Standard SF8/BW250, Messaggio SF12/BW125. Si usa anche per tornare a LoRa
// dopo una prova FSK (fskEnd): begin() rifa' il reset del chip.
bool RadioLora::initLora() {
  const lcr::Profile& prof = lcr::profileFor(settings_.profile);
  // La sync word e' quella del profilo (0 = LCR_SYNC_WORD): l'escursione MeshCom usa quella della rete a pacchetto (0x2B)
  // SX1262: stessa sync word logica (RadioLib 7.5.0 mappa 0x4C su 0x44C4, compatibile con l'SX1278) e TCXO sul DIO3
  int state = radio_->begin(settings_.freqMhz(), prof.bwKhz, prof.sf, prof.cr, prof.syncWord ? prof.syncWord : LCR_SYNC_WORD,
                            static_cast<int8_t>(settings_.powerDbm), prof.preambleSymbols
#if defined(RADIO_SX1262)
                            , LCR_TCXO_VOLTAGE
#endif
  );
  if (state != RADIOLIB_ERR_NONE) {
    lastError_ = state;
    return false;
  }
  rxMhz_ = settings_.freqMhz();
  radio_->setCRC(true);
  radio_->explicitHeader();
  radio_->setPacketReceivedAction(onPacketIsr);
  startReceive();
  return true;
}

bool RadioLora::fskBegin(float mhz, float kbps, float devKhz, int ramp, int shape) {
  // preambolo 0, nessuna parola di sincronismo, nessun CRC: il pacchetto e' solo il suo contenuto
#if defined(RADIO_SX1262)
  // SX1262: la banda di ricezione deve essere una di quelle ammesse (117,3 e' la piu' vicina ai 125 kHz dell'SX1278) e l'ultimo parametro
  // e' la tensione del TCXO, non l'OOK come sull'SX1278
  int state = radio_->beginFSK(mhz, kbps, devKhz, 117.3f, static_cast<int8_t>(settings_.powerDbm), 0, LCR_TCXO_VOLTAGE);
#else
  int state = radio_->beginFSK(mhz, kbps, devKhz, 125.0f, static_cast<int8_t>(settings_.powerDbm), 0, false);
#endif
  if (state == RADIOLIB_ERR_NONE) {
    uint8_t unused = 0;
    state = radio_->setSyncWord(&unused, 0);
  }
  if (state == RADIOLIB_ERR_NONE) state = radio_->setCRC(false);
  if (state == RADIOLIB_ERR_NONE && shape != 0) {
    state = radio_->setDataShaping(shape == 3 ? RADIOLIB_SHAPING_0_3 : (shape == 5 ? RADIOLIB_SHAPING_0_5 : RADIOLIB_SHAPING_1_0));
  }
#if !defined(RADIO_SX1262)
  if (state == RADIOLIB_ERR_NONE && ramp >= 0) {  // salita della potenza: 4 bit bassi di PaRamp (da verificare sul datasheet: 0 = la piu' lenta)
    state = module_->SPIsetRegValue(RADIOLIB_SX127X_REG_PA_RAMP, static_cast<uint8_t>(ramp), 3, 0);
  }
#else
  (void)ramp;  // l'SX1262 ha una salita fissa (200 us, RadioLib): il parametro non c'e'
#endif
  if (state != RADIOLIB_ERR_NONE) lastError_ = state;
  return state == RADIOLIB_ERR_NONE;
}

bool RadioLora::fskSend(uint8_t pattern, uint16_t bytes) {
  static uint8_t buf[255];
  if (bytes == 0 || bytes > sizeof(buf)) return false;
  memset(buf, pattern, bytes);
  int state = radio_->fixedPacketLengthMode(static_cast<uint8_t>(bytes));
  if (state == RADIOLIB_ERR_NONE) {
    txActive_ = true;
    state = radio_->transmit(buf, bytes);  // bloccante: dura bytes * 8 / bit rate
    txActive_ = false;
  }
  if (state != RADIOLIB_ERR_NONE) lastError_ = state;
  return state == RADIOLIB_ERR_NONE;
}

void RadioLora::fskEnd() {
  radio_->standby();
  initLora();
}

// Porta frequenza, banda, SF, CR, sync word e preambolo ai valori dati (i setter di RadioLib mettono la radio in standby e ricalcolano da
// soli l'ottimizzazione per le basse velocita'). Il CRC e l'intestazione esplicita restano come da begin().
bool RadioLora::applyParams(const lcr::Profile& prof, float mhz) {
  radio_->standby();
  int state = setFreq(mhz);
  if (state == RADIOLIB_ERR_NONE) rxMhz_ = mhz;
  if (state == RADIOLIB_ERR_NONE) state = radio_->setBandwidth(prof.bwKhz);
  if (state == RADIOLIB_ERR_NONE) state = radio_->setSpreadingFactor(prof.sf);
  if (state == RADIOLIB_ERR_NONE) state = radio_->setCodingRate(prof.cr);
  if (state == RADIOLIB_ERR_NONE) state = radio_->setSyncWord(prof.syncWord ? prof.syncWord : LCR_SYNC_WORD);
  if (state == RADIOLIB_ERR_NONE) state = radio_->setPreambleLength(prof.preambleSymbols);
  if (state != RADIOLIB_ERR_NONE) lastError_ = state;
  return state == RADIOLIB_ERR_NONE;
}

bool RadioLora::setMeshcom(bool on) {
  const lcr::Profile& primary = lcr::profileFor(settings_.profile);
  bool ok;
  meshcomOn_ = on;
  if (on) {
    ok = applyParams(lcr::meshcomProfile(), static_cast<float>(settings_.freqMcKhz) / 1000.0f);
    if (!ok) applyParams(primary, settings_.freqMhz());  // non e' riuscita: si torna com'era
  } else {
    ok = applyParams(primary, settings_.freqMhz());
  }
  startReceive();
  return ok;
}

bool RadioLora::setParams(const lcr::Profile& prof, uint32_t freqKhz) {
  const bool ok = applyParams(prof, static_cast<float>(freqKhz) / 1000.0f);
  startReceive();
  return ok;
}

void RadioLora::startReceive() {
  rxFlag_ = false;
  const int state = radio_->startReceive();
  if (state != RADIOLIB_ERR_NONE) lastError_ = state;
}

bool RadioLora::channelBusy() {
  if (rxFlag_) return true;  // un pacchetto e' appena arrivato: non e' il momento di trasmettere
  const int r = radio_->scanChannel();  // CAD
  // SX1278: RADIOLIB_PREAMBLE_DETECTED; SX1262: RADIOLIB_LORA_DETECTED (RadioLib 7.5.0, SX126x::getChannelScanResult)
  const bool busy = (r == RADIOLIB_PREAMBLE_DETECTED) || (r == RADIOLIB_LORA_DETECTED);
  startReceive();  // il CAD lascia la radio in standby
  return busy;
}

bool RadioLora::send(const uint8_t* data, size_t len) {
  // Shift (via ripetitore): si trasmette su FREQ + shift e subito dopo si torna ad ascoltare su FREQ. Non durante l'escursione MeshCom.
  const bool shifted = txShiftKhz_ != 0 && !meshcomOn_;
  if (shifted) {
    radio_->standby();
    const int fs = setFreq(rxMhz_ + static_cast<float>(txShiftKhz_) / 1000.0f);
    if (fs != RADIOLIB_ERR_NONE) {  // frequenza non accettata: non si trasmette sul canale sbagliato
      lastError_ = fs;
      setFreq(rxMhz_);
      startReceive();
      return false;
    }
  }
  txActive_ = true;
  const int state = radio_->transmit(data, len);  // bloccante
  txActive_ = false;
  if (state != RADIOLIB_ERR_NONE) lastError_ = state;
  if (shifted) {
    const int fr = setFreq(rxMhz_);
    if (fr != RADIOLIB_ERR_NONE) lastError_ = fr;
  }
  // Dopo la TX la radio e' in standby: si torna in ascolto.
  startReceive();
  return state == RADIOLIB_ERR_NONE;
}

size_t RadioLora::receive(uint8_t* buf, size_t cap, int* rssi) {
  if (!rxFlag_) return 0;
  rxFlag_ = false;
  const uint32_t delayUs = static_cast<uint32_t>(esp_timer_get_time()) - rxIsrUs_;
  if (delayUs > rxMaxDelayUs_) rxMaxDelayUs_ = delayUs;
  // Piu' di un interrupt per un solo flag: l'altro pacchetto e' stato sovrascritto prima della lettura.
  const uint32_t isrs = rxIsrCount_;
  rxIsrCount_ = 0;
  if (isrs > 1) rxOverwritten_ += isrs - 1;
  const size_t n = radio_->getPacketLength();
  size_t got = 0;
  if (n > 0 && n <= cap) {
    const int state = radio_->readData(buf, n);
    if (state == RADIOLIB_ERR_NONE) {
      got = n;
      rxPackets_++;
      lastRssi_ = radio_->getRSSI();
      lastSnr_ = radio_->getSNR();
      if (rssi) *rssi = static_cast<int>(lastRssi_);
    } else {
      lastError_ = state;  // CRC errato o altro: il pacchetto e' scartato
      if (state == RADIOLIB_ERR_CRC_MISMATCH) rxCrcErrors_++;
    }
  }
  // In ricezione continua la radio resta in RX, ma si riarma per sicurezza.
  const int s = radio_->startReceive();
  if (s != RADIOLIB_ERR_NONE) lastError_ = s;
  return got;
}
