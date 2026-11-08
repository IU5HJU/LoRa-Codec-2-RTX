// Adattatore Codec2 (modo 1200 bps): implementa lcr::ICodec.
// Solo per ESP32 (libreria esp32_codec2): non e' compilato nei test sul PC.
#pragma once

#include <codec2.h>

#include "interfaces.h"
#include "protocol.h"

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

  // Registro dei frame codificati e di quelli decodificati (i primi kLogFrames dopo logReset()), per confrontare in
  // seriale i frame trasmessi con quelli ricevuti ([tx-hex] e [rx-hex]).
  static constexpr size_t kLogFrames = 300;  // 12 s
  void logReset() { encLogN_ = decLogN_ = 0; }
  size_t encLogCount() const { return encLogN_; }
  size_t decLogCount() const { return decLogN_; }
  const uint8_t* encLog() const { return encLog_; }
  const uint8_t* decLog() const { return decLog_; }

 private:
  uint8_t encLog_[kLogFrames * lcr::kFrameBytes];
  uint8_t decLog_[kLogFrames * lcr::kFrameBytes];
  size_t encLogN_ = 0, decLogN_ = 0;
  uint32_t encMaxUs_ = 0, decMaxUs_ = 0, encN_ = 0, decN_ = 0;
  uint64_t encSumUs_ = 0, decSumUs_ = 0;
  struct CODEC2* c2_ = nullptr;
};
