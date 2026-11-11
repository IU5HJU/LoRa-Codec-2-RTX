#include "inbox.h"

#include <cstring>

#include "packet.h"

namespace lcr {

void Inbox::setCallsign(const char* cs) {
  memset(callsign_, 0, sizeof(callsign_));
  if (cs) strncpy(callsign_, cs, kMaxFrom);
}

void Inbox::clear() {
  kind_ = Kind::None;
  frames_ = 0;
  textLen_ = 0;
  text_[0] = 0;
  callsign_[0] = 0;
}

void Inbox::beginVoice() {
  clear();
  memset(lost_, 0, sizeof(lost_));
}

bool Inbox::pushFrame(const uint8_t* frame) {
  if (frames_ >= kMaxTurnFrames) return false;
  memcpy(frameBuf_ + frames_ * kFrameBytes, frame, kFrameBytes);
  lost_[frames_ / 8] &= static_cast<uint8_t>(~(1u << (frames_ % 8)));
  frames_++;
  return true;
}

bool Inbox::pushLost() {
  if (frames_ >= kMaxTurnFrames) return false;
  lost_[frames_ / 8] |= static_cast<uint8_t>(1u << (frames_ % 8));
  frames_++;
  return true;
}

bool Inbox::commitVoice(const char* callsign) {
  if (frames_ == 0) {
    clear();
    return false;
  }
  setCallsign(callsign);
  kind_ = Kind::Voice;
  return true;
}

bool Inbox::frameAt(size_t i, uint8_t* out) const {
  if (kind_ != Kind::Voice || i >= frames_) return false;
  if (lost_[i / 8] & (1u << (i % 8))) return false;
  memcpy(out, frameBuf_ + i * kFrameBytes, kFrameBytes);
  return true;
}

bool Inbox::setText(const char* callsign, const char* text, size_t len) {
  if (!isValidText(text, len)) return false;
  clear();
  setCallsign(callsign);
  memcpy(text_, text, len);
  text_[len] = 0;
  textLen_ = len;
  kind_ = Kind::Text;
  return true;
}

}  // namespace lcr
