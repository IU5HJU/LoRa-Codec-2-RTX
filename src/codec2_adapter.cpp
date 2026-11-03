#include "codec2_adapter.h"

#include "protocol.h"

Codec2Adapter::~Codec2Adapter() {
  if (c2_) codec2_destroy(c2_);
}

bool Codec2Adapter::begin() {
  c2_ = codec2_create(CODEC2_MODE_1200);
  if (!c2_) return false;
  return codec2_samples_per_frame(c2_) == static_cast<int>(lcr::kFrameSamples) &&
         codec2_bytes_per_frame(c2_) == static_cast<int>(lcr::kFrameBytes);
}

void Codec2Adapter::encode(const int16_t* pcm, uint8_t* frame) {
  codec2_encode(c2_, frame, const_cast<int16_t*>(pcm));
}

void Codec2Adapter::decode(const uint8_t* frame, int16_t* pcm) { codec2_decode(c2_, pcm, frame); }
