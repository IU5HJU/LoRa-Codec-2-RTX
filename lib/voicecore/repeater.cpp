#include "repeater.h"

namespace lcr {

void Repeater::reset() {
  for (size_t i = 0; i < kDedupSlots; i++) slots_[i].used = false;
  next_ = 0;
  haveSession_ = false;
  curSession_ = 0;
  curStartMs_ = 0;
  stats_ = Stats();
}

bool Repeater::seenRecently(uint16_t session, uint8_t seq, uint32_t nowMs) const {
  for (size_t i = 0; i < kDedupSlots; i++) {
    const Slot& s = slots_[i];
    if (s.used && s.session == session && s.seq == seq && nowMs - s.atMs < kDedupWindowMs) return true;
  }
  return false;
}

void Repeater::remember(uint16_t session, uint8_t seq, uint32_t nowMs) {
  Slot& s = slots_[next_];
  s.used = true;
  s.session = session;
  s.seq = seq;
  s.atMs = nowMs;
  next_ = (next_ + 1) % kDedupSlots;
}

Repeater::Verdict Repeater::onPacket(const Packet& in, uint32_t nowMs, Packet& out) {
  if (!enabled_) {
    stats_.disabled++;
    return Verdict::Disabled;
  }
  if (in.flags & kFlagRelayed) {
    stats_.alreadyRelayed++;
    return Verdict::AlreadyRelayed;
  }
  if (in.mode > 15 || !(modeMask_ & (1u << in.mode))) {
    stats_.modeNotRepeated++;
    return Verdict::ModeNotRepeated;
  }
  if (seenRecently(in.session, in.seq, nowMs)) {
    stats_.duplicate++;
    return Verdict::Duplicate;
  }
  // Durata della sessione: parte al primo pacchetto di una sessione nuova.
  if (!haveSession_ || in.session != curSession_) {
    haveSession_ = true;
    curSession_ = in.session;
    curStartMs_ = nowMs;
  } else if (nowMs - curStartMs_ > maxSessionMs_) {
    stats_.sessionTooLong++;
    return Verdict::SessionTooLong;
  }
  remember(in.session, in.seq, nowMs);
  out = in;
  out.flags = static_cast<uint8_t>(in.flags | kFlagRelayed);
  stats_.forwarded++;
  return Verdict::Forward;
}

}  // namespace lcr
