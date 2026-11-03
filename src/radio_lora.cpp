#include "radio_lora.h"

#include <SPI.h>

#include "board_pins.h"
#include "user_config.h"

volatile bool RadioLora::rxFlag_ = false;
volatile bool RadioLora::txActive_ = false;

void IRAM_ATTR RadioLora::onPacketIsr() {
  if (!txActive_) rxFlag_ = true;  // durante la TX DIO0 segnala "trasmesso", non "ricevuto"
}

bool RadioLora::begin() {
  SPI.begin(pins::kRadioSclk, pins::kRadioMiso, pins::kRadioMosi, pins::kRadioCs);
  radio_ = new SX1278(new Module(pins::kRadioCs, pins::kRadioDio0, pins::kRadioRst, pins::kRadioDio1, SPI));

  int state = radio_->begin(LCR_FREQUENCY_MHZ, LCR_BANDWIDTH_KHZ, LCR_SPREADING_FACTOR, LCR_CODING_RATE,
                            LCR_SYNC_WORD, LCR_TX_POWER_DBM, LCR_PREAMBLE_SYMBOLS);
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
  const size_t n = radio_->getPacketLength();
  size_t got = 0;
  if (n > 0 && n <= cap) {
    const int state = radio_->readData(buf, n);
    if (state == RADIOLIB_ERR_NONE) {
      got = n;
      lastRssi_ = radio_->getRSSI();
      lastSnr_ = radio_->getSNR();
      if (rssi) *rssi = static_cast<int>(lastRssi_);
    } else {
      lastError_ = state;  // CRC errato o altro: il pacchetto e' scartato
    }
  }
  // In ricezione continua la radio resta in RX, ma si riarma per sicurezza.
  const int s = radio_->startReceive();
  if (s != RADIOLIB_ERR_NONE) lastError_ = s;
  return got;
}
