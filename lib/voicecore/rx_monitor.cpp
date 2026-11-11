#include "rx_monitor.h"

namespace lcr {

RxMonitor::RxMonitor(uint8_t profileId, ICodec& codec, IAudioOut& out) : prof_(profileFor(profileId)), codec_(codec), out_(out) {
  RxStream::Config rc;
  rc.prebufferFrames = prof_.rxPrebufferFrames;
  rc.prebufferTimeoutMs = prof_.rxPrebufferTimeoutMs;
  rc.sessionTimeoutMs = prof_.rxSessionTimeoutMs;
  rx_.configure(rc);
}

bool RxMonitor::onPacket(const Packet& p, uint32_t nowMs) {
  if (p.mode != prof_.codecMode) return false;  // testo, prove e altri modi: non si ascoltano
  if (!rx_.onPacket(p, nowMs)) return false;
  if (!playing_) {
    playing_ = true;
    nextPlayMs_ = nowMs;
    sessions_++;
  }
  return true;
}

void RxMonitor::tick(uint32_t nowMs) {
  if (!playing_ || static_cast<int32_t>(nowMs - nextPlayMs_) < 0) return;
  const RxStream::Out o = rx_.nextFrame(nowMs);
  switch (o.kind) {
    case RxStream::Kind::Wait:
      nextPlayMs_ = nowMs;
      return;
    case RxStream::Kind::Frame: {
      int16_t pcm[kFrameSamples];
      codec_.decode(o.frame, pcm);
      out_.playFrame(pcm);
      framesPlayed_++;
      break;
    }
    case RxStream::Kind::Lost:
      out_.playLost();
      framesLost_++;
      break;
    case RxStream::Kind::End:
      playing_ = false;
      return;
  }
  nextPlayMs_ += kFrameMs;
  if (nowMs > nextPlayMs_ + 5 * kFrameMs) nextPlayMs_ = nowMs + kFrameMs;
}

}  // namespace lcr
