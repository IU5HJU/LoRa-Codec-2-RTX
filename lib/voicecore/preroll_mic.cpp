#include "preroll_mic.h"

#include <cstring>

namespace lcr {

PrerollMic::PrerollMic(size_t prerollFrames) : preroll_(prerollFrames > kCapacity ? kCapacity : prerollFrames) {
  memset(frames_, 0, sizeof(frames_));
}

void PrerollMic::trimTo(size_t n) {
  while (count_ > n) {
    head_ = (head_ + 1) % kCapacity;
    count_--;
  }
}

void PrerollMic::feed(const int16_t* pcm) {
  if (count_ == kCapacity) {
    // Ring pieno: si scarta il frame piu' vecchio (in registrazione e' una perdita).
    if (recording_) overruns_++;
    trimTo(kCapacity - 1);
  }
  memcpy(frames_[(head_ + count_) % kCapacity], pcm, kFrameSamples * sizeof(int16_t));
  count_++;
  if (!recording_) trimTo(preroll_);
}

void PrerollMic::start() {
  trimTo(preroll_);
  recording_ = true;
}

void PrerollMic::stop() {
  recording_ = false;
  trimTo(0);
}

bool PrerollMic::readFrame(int16_t* pcm) {
  if (!recording_ || count_ == 0) return false;
  memcpy(pcm, frames_[head_], kFrameSamples * sizeof(int16_t));
  head_ = (head_ + 1) % kCapacity;
  count_--;
  return true;
}

}  // namespace lcr
