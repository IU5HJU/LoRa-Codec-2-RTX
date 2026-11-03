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

 private:
  struct CODEC2* c2_ = nullptr;
};
