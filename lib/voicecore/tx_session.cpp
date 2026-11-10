#include "tx_session.h"

#include <cstring>

namespace lcr {

TxSession::TxSession(const char* callsign, uint16_t session, bool fec, uint8_t mode)
    : session_(session), fec_(fec), mode_(mode), frameBytes_(frameBytesFor(mode)) {
  if (callsign) {
    strncpy(callsign_, callsign, kMaxCallsign);
    callsign_[kMaxCallsign] = 0;
  }
  parity_.reset();
}

size_t TxSession::expectedPackets(size_t frames, bool fec) {
  const size_t data = 1 + frames / kFramesPerPacket + 1;  // START, pacchetti pieni, END con i frame restanti (anche 0)
  return fec ? data + (data + kParityGroup - 1) / kParityGroup : data;
}

bool TxSession::enqueue(const Packet& p) {
  if (qCount_ >= kQueue) {
    overflow_ = true;
    return false;
  }
  q_[(qHead_ + qCount_) % kQueue] = p;
  qCount_++;
  return true;
}

bool TxSession::popPacket(Packet& out) {
  if (qCount_ == 0) return false;
  out = q_[qHead_];
  qHead_ = (qHead_ + 1) % kQueue;
  qCount_--;
  return true;
}

bool TxSession::emitData(uint8_t extraFlags) {
  Packet p;
  p.mode = mode_;
  p.session = session_;
  p.seq = seq_;
  p.flags = extraFlags | (fec_ ? 0 : kFlagNoFec) | (seq_ == 0 ? kFlagStart : 0);

  const bool withCallsign = callsign_[0] != 0 && (seq_ == 0 || (extraFlags & kFlagEnd) ||
                                                   static_cast<uint8_t>(seq_ - lastCallsignSeq_) >= kCallsignRepeatPackets);
  if (withCallsign) {
    memcpy(p.callsign, callsign_, sizeof(p.callsign));
    lastCallsignSeq_ = seq_;
    // La posizione viaggia solo nel pacchetto START (decisione del 09/11/2026: i turni sono brevi).
    if (seq_ == 0 && position_.valid) p.position = position_;
  }
  if (seq_ == 0 && expectedTotal_) p.totalPackets = expectedTotal_;

  p.payloadLen = static_cast<uint8_t>(frameCount_ * frameBytes_);
  if (p.payloadLen) memcpy(p.payload, frames_, p.payloadLen);
  frameCount_ = 0;

  if (!enqueue(p)) return false;
  seq_++;
  if (fec_) {
    parity_.add(p);
    if (parity_.count() == kParityGroup) emitParity();
  }
  return true;
}

void TxSession::emitParity() {
  Packet p;
  parity_.build(p, session_, mode_, seq_);
  if (enqueue(p)) seq_++;
  parity_.reset();
}

void TxSession::begin() {
  if (begun_) return;
  begun_ = true;
  emitData(kFlagStart);
}

bool TxSession::pushFrame(const uint8_t* frame) {
  if (finished_) return false;
  if (!begun_) begin();
  if (seq_ >= kTxSeqLimit || overflow_) return false;
  memcpy(frames_ + frameCount_ * frameBytes_, frame, frameBytes_);
  frameCount_++;
  if (frameCount_ == kFramesPerPacket) return emitData(0);
  return true;
}

void TxSession::finish() {
  if (finished_) return;
  if (!begun_) begin();
  finished_ = true;
  emitData(kFlagEnd);
  if (fec_ && parity_.count() > 0) emitParity();
}

}  // namespace lcr
