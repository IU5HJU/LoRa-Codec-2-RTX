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

bool RadioLora::begin(const lcr::Settings& settings) {
  settings_ = settings;
  SPI.begin(pins::kRadioSclk, pins::kRadioMiso, pins::kRadioMosi, pins::kRadioCs);
  radio_ = new SX1278(new Module(pins::kRadioCs, pins::kRadioDio0, pins::kRadioRst, pins::kRadioDio1, SPI));

  // I parametri LoRa vengono dal profilo scelto (profile.h): Standard SF8/BW250, Messaggio SF12/BW125.
  const lcr::Profile& prof = lcr::profileFor(settings.profile);
  // La sync word e' quella del profilo (0 = LCR_SYNC_WORD): l'escursione MeshCom usa quella della rete a pacchetto (0x2B)
  int state = radio_->begin(settings.freqMhz(), prof.bwKhz, prof.sf, prof.cr, prof.syncWord ? prof.syncWord : LCR_SYNC_WORD,
                            static_cast<int8_t>(settings.powerDbm), prof.preambleSymbols);
  if (state != RADIOLIB_ERR_NONE) {
    lastError_ = state;
    return false;
  }
  radio_->setCRC(true);
  radio_->explicitHeader();
  radio_->setPacketReceivedAction(onPacketIsr);
  startReceive();
  return true;
}

// Porta frequenza, banda, SF, CR, sync word e preambolo ai valori dati (i setter di RadioLib mettono la radio in standby e ricalcolano da
// soli l'ottimizzazione per le basse velocita'). Il CRC e l'intestazione esplicita restano come da begin().
bool RadioLora::applyParams(const lcr::Profile& prof, float mhz) {
  radio_->standby();
  int state = radio_->setFrequency(mhz);
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
  if (on) {
    ok = applyParams(lcr::meshcomProfile(), static_cast<float>(settings_.freqMcKhz) / 1000.0f);
    if (!ok) applyParams(primary, settings_.freqMhz());  // non e' riuscita: si torna com'era
  } else {
    ok = applyParams(primary, settings_.freqMhz());
  }
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
  const bool busy = (r == RADIOLIB_PREAMBLE_DETECTED);
  startReceive();  // il CAD lascia la radio in standby
  return busy;
}

bool RadioLora::send(const uint8_t* data, size_t len) {
  txActive_ = true;
  const int state = radio_->transmit(data, len);  // bloccante
  txActive_ = false;
  if (state != RADIOLIB_ERR_NONE) lastError_ = state;
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
