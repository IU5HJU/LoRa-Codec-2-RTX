#include "rx_stream.h"

#include <cstring>

namespace lcr {

void RxStream::reset() {
  phase_ = Phase::Idle;
  session_ = 0;
  fec_ = true;
  memset(callsign_, 0, sizeof(callsign_));
  firstRxMs_ = 0;
  lastRxMs_ = 0;
  endSeq_ = -1;
  highest_ = 0;
  cursor_ = 0;
  curFrame_ = 0;
  lostCount_ = 0;
  recovered_ = 0;
  lostFrames_ = 0;
  for (size_t i = 0; i < kMaxPackets; i++) {
    slots_[i].have = false;
    slots_[i].parityDone = false;
  }
}

bool RxStream::onPacket(const Packet& p, uint32_t nowMs) {
  if (p.seq >= kMaxPackets) return false;

  if (phase_ != Phase::Idle && p.session != session_) {
    const bool stale = (phase_ == Phase::Done) || (nowMs - lastRxMs_ > kSessionTimeoutMs);
    if (!stale) return false;
    reset();
  }
  if (phase_ == Phase::Done) return false;  // ritardatari della trasmissione conclusa

  if (phase_ == Phase::Idle) {
    session_ = p.session;
    phase_ = Phase::Buffering;
    firstRxMs_ = nowMs;
    fec_ = (p.flags & kFlagNoFec) == 0 || p.isParity();
    cursor_ = p.seq;
  } else if (phase_ == Phase::Buffering && p.seq < cursor_) {
    cursor_ = p.seq;
  }
  lastRxMs_ = nowMs;

  if (p.hasCallsign()) memcpy(callsign_, p.callsign, sizeof(callsign_));
  if (p.seq > highest_) highest_ = p.seq;

  if (!slots_[p.seq].have) {
    slots_[p.seq].have = true;
    slots_[p.seq].parityDone = false;
    slots_[p.seq].pkt = p;
    if ((p.flags & kFlagEnd) && !p.isParity()) endSeq_ = p.seq;
  }
  tryRecoverAll();
  return true;
}

void RxStream::tryRecoverAll() {
  for (size_t s = 0; s <= highest_ && s < kMaxPackets; s++) {
    Slot& ps = slots_[s];
    if (!ps.have || !ps.pkt.isParity() || ps.parityDone) continue;
    const size_t count = ps.pkt.payload[0];
    if (count == 0 || count > kParityGroup || s < count) {
      ps.parityDone = true;
      continue;
    }
    const Packet* group[kParityGroup];
    size_t missing = 0;
    size_t missingSeq = 0;
    for (size_t i = 0; i < count; i++) {
      const Slot& d = slots_[s - count + i];
      if (d.have && !d.pkt.isParity()) {
        group[i] = &d.pkt;
      } else {
        group[i] = nullptr;
        missing++;
        missingSeq = s - count + i;
      }
    }
    if (missing == 0) {
      ps.parityDone = true;
    } else if (missing == 1) {
      Packet rec;
      if (recoverFromParity(ps.pkt, group, count, rec)) {
        slots_[missingSeq].have = true;
        slots_[missingSeq].parityDone = false;
        slots_[missingSeq].pkt = rec;
        recovered_++;
        if (rec.flags & kFlagEnd) endSeq_ = rec.seq;
      }
      ps.parityDone = true;
    }
    // missing > 1: non recuperabile, si riprova se arrivano altri pacchetti.
  }
}

size_t RxStream::paritySeqFor(size_t seq) const {
  const size_t groupStart = seq - (seq % kParityPeriod);
  size_t ps = groupStart + kParityGroup;
  // Gruppo finale parziale: la parita' segue subito il pacchetto END.
  if (endSeq_ >= 0 && seq <= static_cast<size_t>(endSeq_) && static_cast<size_t>(endSeq_) < ps) ps = endSeq_ + 1;
  return ps;
}

// Un pacchetto mancante e' definitivamente perso solo quando la sua parita'
// e' arrivata (e non e' bastata) o e' stata saltata.
bool RxStream::resolved(size_t seq) const {
  if (!fec_) return highest_ > seq;
  const size_t ps = paritySeqFor(seq);
  if (ps >= kMaxPackets) return highest_ > seq;
  return slots_[ps].have || highest_ > ps;
}

size_t RxStream::bufferedFrames() const {
  size_t total = 0;
  for (size_t s = cursor_; s < kMaxPackets; s++) {
    if (endSeq_ >= 0 && s > static_cast<size_t>(endSeq_)) break;
    if (isParitySeq(s)) continue;
    if (!slots_[s].have) break;
    total += slots_[s].pkt.payloadLen / kFrameBytes;
  }
  return total;
}

void RxStream::finish() { phase_ = Phase::Done; }

RxStream::Out RxStream::nextFrame(uint32_t nowMs) {
  const Out wait{Kind::Wait, nullptr};
  const Out end{Kind::End, nullptr};

  if (phase_ == Phase::Idle) return wait;
  if (phase_ == Phase::Done) return end;

  if (phase_ == Phase::Buffering) {
    if (bufferedFrames() >= kPrebufferFrames || endSeq_ >= 0 || nowMs - firstRxMs_ >= kPrebufferTimeoutMs) {
      phase_ = Phase::Playing;
    } else {
      return wait;
    }
  }

  for (;;) {
    if (cursor_ >= kMaxPackets || (endSeq_ >= 0 && cursor_ > static_cast<size_t>(endSeq_))) {
      finish();
      return end;
    }
    if (isParitySeq(cursor_)) {
      cursor_++;
      curFrame_ = 0;
      continue;
    }
    const Slot& s = slots_[cursor_];
    if (s.have) {
      const size_t n = s.pkt.payloadLen / kFrameBytes;
      if (curFrame_ < n) {
        const uint8_t* f = s.pkt.payload + curFrame_ * kFrameBytes;
        curFrame_++;
        return Out{Kind::Frame, f};
      }
      cursor_++;
      curFrame_ = 0;
      continue;
    }
    if (resolved(cursor_)) {
      if (lostCount_ < kFramesPerPacket) {
        lostCount_++;
        lostFrames_++;
        return Out{Kind::Lost, nullptr};
      }
      lostCount_ = 0;
      cursor_++;
      curFrame_ = 0;
      continue;
    }
    // Pacchetto non ancora arrivato: attesa, con chiusura per inattivita'.
    if (nowMs - lastRxMs_ > kSessionTimeoutMs) {
      finish();
      return end;
    }
    return wait;
  }
}

}  // namespace lcr
