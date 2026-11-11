#include "message_store.h"

#include <cstring>

#include "tx_session.h"

namespace lcr {

MessageStore::MessageStore() { reset(); }

void MessageStore::reset() {
  cur_.active = false;
  doneValid_ = false;
  readyN_ = 0;
  stats_ = Stats();
}

void MessageStore::abort() { cur_.active = false; }

MessageStore::Result MessageStore::onPacket(const Packet& p, uint32_t nowMs) {
  if (p.flags & kFlagRelayed) return Result::Ignored;      // gia' ripetuto da un ripetitore
  if (p.mode != kModeCodec2_1200) return Result::Ignored;  // i messaggi viaggiano in Codec2 1200
  if (!cur_.active && doneValid_ && p.session == doneSession_ && nowMs - doneMs_ <= kSilenceMs) return Result::Ignored;  // coda del messaggio appena completato
  if (cur_.active && p.session != cur_.session) {
    if (nowMs - cur_.lastMs <= kSilenceMs) return Result::Ignored;  // un'altra sessione mentre se ne riceve una: si ignora
    stats_.abandoned++;
    abort();
  }
  if (p.seq >= kMaxPacketsPerMessage) {  // oltre il limite di lunghezza
    if (cur_.active && p.session == cur_.session) abort();
    stats_.tooLong++;
    return Result::TooLong;
  }
  if (!cur_.active) {
    cur_.active = true;
    cur_.session = p.session;
    cur_.fec = (p.flags & kFlagNoFec) == 0 || p.isParity();
    cur_.total = 0;
    cur_.endSeq = -1;
    cur_.highest = 0;
    for (size_t i = 0; i < kMaxPacketsPerMessage; i++) {
      cur_.have[i] = false;
      cur_.parityDone[i] = false;
    }
  }
  cur_.lastMs = nowMs;
  if (cur_.have[p.seq]) return Result::Duplicate;
  cur_.have[p.seq] = true;
  cur_.parityDone[p.seq] = false;
  cur_.pk[p.seq] = p;
  if (p.seq > cur_.highest) cur_.highest = p.seq;
  if ((p.flags & kFlagEnd) && !p.isParity()) cur_.endSeq = p.seq;

  const size_t maxTotal = TxSession::expectedPackets(kMaxVoiceMs / kFrameMs, true);
  if (p.hasTotal()) {
    cur_.total = p.totalPackets;
    if (cur_.total > maxTotal) {
      stats_.tooLong++;
      abort();
      return Result::TooLong;
    }
  }
  if (cur_.endSeq >= 0) {
    const size_t t = static_cast<size_t>(cur_.endSeq) + (cur_.fec ? 2 : 1);
    if (t > maxTotal) {
      stats_.tooLong++;
      abort();
      return Result::TooLong;
    }
  }

  recoverAll();
  if (complete()) {
    finish(nowMs);
    return Result::Complete;
  }
  return Result::Accepted;
}

void MessageStore::recoverAll() {
  for (size_t s = 0; s <= cur_.highest && s < kMaxPacketsPerMessage; s++) {
    if (!cur_.have[s] || !cur_.pk[s].isParity() || cur_.parityDone[s]) continue;
    const size_t count = cur_.pk[s].payload[0];
    if (count == 0 || count > kParityGroup || s < count) {
      cur_.parityDone[s] = true;
      continue;
    }
    const Packet* group[kParityGroup];
    size_t missing = 0, missingSeq = 0;
    for (size_t i = 0; i < count; i++) {
      const size_t idx = s - count + i;
      if (cur_.have[idx] && !cur_.pk[idx].isParity()) {
        group[i] = &cur_.pk[idx];
      } else {
        group[i] = nullptr;
        missing++;
        missingSeq = idx;
      }
    }
    if (missing == 0) {
      cur_.parityDone[s] = true;
    } else if (missing == 1) {
      Packet rec;
      if (recoverFromParity(cur_.pk[s], group, count, rec)) {
        cur_.have[missingSeq] = true;
        cur_.parityDone[missingSeq] = false;
        cur_.pk[missingSeq] = rec;
        stats_.recovered++;
        if (rec.flags & kFlagEnd) cur_.endSeq = static_cast<int>(rec.seq);
      }
      cur_.parityDone[s] = true;
    }
  }
}

bool MessageStore::complete() const {
  if (cur_.endSeq < 0) return false;
  for (size_t s = 0; s <= static_cast<size_t>(cur_.endSeq); s++) {
    if (isParitySeq(s)) continue;
    if (!cur_.have[s]) return false;
  }
  return true;
}

void MessageStore::finish(uint32_t nowMs) {
  // Si fa posto se la coda e' piena: si scarta il messaggio piu' vecchio.
  if (readyN_ >= kQueue) {
    for (size_t i = 1; i < readyN_; i++) ready_[i - 1] = ready_[i];
    readyN_--;
    stats_.droppedOld++;
  }
  Msg& m = ready_[readyN_];
  m.completedMs = nowMs;
  const size_t endSeq = static_cast<size_t>(cur_.endSeq);

  // Nominativo: se lo START e' stato ricostruito dalla parita' non lo ha, si prende da un altro pacchetto che lo porta.
  char callsign[kMaxCallsign + 1] = {0};
  for (size_t s = 0; s <= endSeq; s++) {
    if (!isParitySeq(s) && cur_.have[s] && cur_.pk[s].hasCallsign()) {
      memcpy(callsign, cur_.pk[s].callsign, sizeof(callsign));
      break;
    }
  }
  const uint8_t mode = cur_.pk[0].mode;
  size_t n = 0;
  ParityAccumulator acc;
  size_t groupFill = 0;
  auto emitParity = [&](size_t seq) {
    Packet par;
    acc.build(par, cur_.session, mode, static_cast<uint8_t>(seq));
    par.flags = static_cast<uint8_t>(par.flags | kFlagRelayed);
    m.pk[n++] = par;
    acc.reset();
    groupFill = 0;
  };
  for (size_t s = 0; s <= endSeq; s++) {
    if (isParitySeq(s)) {
      if (cur_.fec) emitParity(s);
      continue;
    }
    Packet d = cur_.pk[s];
    d.flags = static_cast<uint8_t>(d.flags | kFlagRelayed);
    m.pk[n++] = d;
    if (cur_.fec) {
      acc.add(d);
      groupFill++;
    }
  }
  if (cur_.fec && groupFill > 0) emitParity(endSeq + 1);  // parita' del gruppo finale parziale
  m.n = n;

  // START: nominativo e totale dei pacchetti dichiarato (anche se lo START e' stato ricostruito).
  if (m.pk[0].flags & kFlagStart) {
    if (!m.pk[0].hasCallsign() && callsign[0]) memcpy(m.pk[0].callsign, callsign, sizeof(callsign));
    if (m.pk[0].hasCallsign()) m.pk[0].totalPackets = static_cast<uint8_t>(n);
  }
  readyN_++;
  stats_.completed++;
  doneValid_ = true;
  doneSession_ = cur_.session;
  doneMs_ = nowMs;
  cur_.active = false;
}

void MessageStore::tick(uint32_t nowMs) {
  if (cur_.active && nowMs - cur_.lastMs > kSilenceMs) {
    stats_.abandoned++;
    abort();
  }
  // Messaggi scaduti (i piu' vecchi stanno in testa).
  while (readyN_ > 0 && nowMs - ready_[0].completedMs > kExpireMs) {
    popReady();
    stats_.droppedOld++;
  }
}

bool MessageStore::readyPacket(size_t i, uint16_t newSession, Packet& out) const {
  if (readyN_ == 0 || i >= ready_[0].n) return false;
  out = ready_[0].pk[i];
  out.session = newSession;
  return true;
}

uint32_t MessageStore::readyAirtimeMs(const Profile& prof) const {
  if (readyN_ == 0) return 0;
  double t = 0;
  for (size_t i = 0; i < ready_[0].n; i++) {
    uint8_t buf[kMaxWireBytes];
    const size_t bytes = encodePacket(ready_[0].pk[i], buf, sizeof(buf));
    t += airtimeMs(bytes, prof);
  }
  return static_cast<uint32_t>(t + 0.5);
}

void MessageStore::popReady() {
  if (readyN_ == 0) return;
  for (size_t i = 1; i < readyN_; i++) ready_[i - 1] = ready_[i];
  readyN_--;
}

}  // namespace lcr
