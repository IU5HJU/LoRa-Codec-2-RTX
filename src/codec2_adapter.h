// Adattatore Codec2 (modo 1200 bps): implementa lcr::ICodec.
// Solo per ESP32 (libreria esp32_codec2): non e' compilato nei test sul PC.
#pragma once

#include <codec2.h>

#include "interfaces.h"

class Codec2Adapter : public lcr::ICodec {
 public:
  ~Codec2Adapter() override;
  // false se Codec2 non si crea o le dimensioni non sono quelle del protocollo
  // (320 campioni, 6 byte per frame).
  bool begin();

  void encode(const int16_t* pcm, uint8_t* frame) override;
  void decode(const uint8_t* frame, int16_t* pcm) override;

  // Tempo di calcolo (microsecondi) di codifica e decodifica di un frame: ammesso al massimo 40000.
  void resetTiming() { encMaxUs_ = decMaxUs_ = 0; encSumUs_ = decSumUs_ = 0; encN_ = decN_ = 0; }
  uint32_t encodeAvgUs() const { return encN_ ? static_cast<uint32_t>(encSumUs_ / encN_) : 0; }
  uint32_t encodeMaxUs() const { return encMaxUs_; }
  uint32_t decodeAvgUs() const { return decN_ ? static_cast<uint32_t>(decSumUs_ / decN_) : 0; }
  uint32_t decodeMaxUs() const { return decMaxUs_; }

 private:
  uint32_t encMaxUs_ = 0, decMaxUs_ = 0, encN_ = 0, decN_ = 0;
  uint64_t encSumUs_ = 0, decSumUs_ = 0;
  struct CODEC2* c2_ = nullptr;
};
