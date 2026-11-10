#include "codec2_adapter.h"

#include <Arduino.h>

#include "protocol.h"

Codec2Adapter::~Codec2Adapter() {
  if (c2_) codec2_destroy(c2_);
}

bool Codec2Adapter::begin(uint8_t mode) {
  if (c2_) {
    codec2_destroy(c2_);
    c2_ = nullptr;
  }
  c2_ = codec2_create(mode == lcr::kModeCodec2_3200 ? CODEC2_MODE_3200 : CODEC2_MODE_1200);
  if (!c2_) return false;
  frameBytes_ = lcr::frameBytesFor(mode);
  blockSamples_ = static_cast<size_t>(codec2_samples_per_frame(c2_));
  blockBytes_ = static_cast<size_t>(codec2_bytes_per_frame(c2_));
  if (blockSamples_ == 0 || lcr::kFrameSamples % blockSamples_ != 0) return false;
  blocks_ = lcr::kFrameSamples / blockSamples_;
  return blocks_ * blockBytes_ == frameBytes_;
}

void Codec2Adapter::encode(const int16_t* pcm, uint8_t* frame) {
  const uint32_t t0 = micros();
  for (size_t b = 0; b < blocks_; b++)
    codec2_encode(c2_, frame + b * blockBytes_, const_cast<int16_t*>(pcm) + b * blockSamples_);
  const uint32_t dt = micros() - t0;
  if (encLogN_ < kLogFrames) memcpy(encLog_ + (encLogN_++) * lcr::kMaxFrameBytes, frame, frameBytes_);
  encSumUs_ += dt;
  encN_++;
  if (dt > encMaxUs_) encMaxUs_ = dt;
}

void Codec2Adapter::decode(const uint8_t* frame, int16_t* pcm) {
  const uint32_t t0 = micros();
  for (size_t b = 0; b < blocks_; b++) codec2_decode(c2_, pcm + b * blockSamples_, frame + b * blockBytes_);
  const uint32_t dt = micros() - t0;
  if (decLogN_ < kLogFrames) memcpy(decLog_ + (decLogN_++) * lcr::kMaxFrameBytes, frame, frameBytes_);
  decSumUs_ += dt;
  decN_++;
  if (dt > decMaxUs_) decMaxUs_ = dt;
}
