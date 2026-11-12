#include "sysop.h"

#include <cstring>

#include "sha256.h"

namespace lcr {

namespace {

// Firma dei primi 'n' byte del pacchetto codificato (payload senza la firma) con la chiave.
bool sign(Packet& p, size_t payloadWithoutSig, const uint8_t key[kSysopKeyBytes], uint8_t sig[kSysopSigBytes]) {
  Packet q = p;
  q.payloadLen = static_cast<uint8_t>(payloadWithoutSig);
  // encodePacket vuole i due soli payload validi: il messaggio da firmare si costruisce a mano
  uint8_t wire[kMaxWireBytes];
  // intestazione + nominativo come in encodePacket, poi il payload senza firma
  size_t pos = 0;
  const size_t csLen = strlen(q.callsign);
  wire[pos++] = static_cast<uint8_t>((kProtocolVersion << 4) | (q.mode & 0x0F));
  wire[pos++] = static_cast<uint8_t>(q.session >> 8);
  wire[pos++] = static_cast<uint8_t>(q.session & 0xFF);
  wire[pos++] = q.seq;
  wire[pos++] = static_cast<uint8_t>(q.flags | kFlagCallsign);
  wire[pos++] = static_cast<uint8_t>(csLen);
  memcpy(wire + pos, q.callsign, csLen);
  pos += csLen;
  memcpy(wire + pos, q.payload, payloadWithoutSig);
  pos += payloadWithoutSig;
  uint8_t mac[kSha256Bytes];
  hmacSha256(key, kSysopKeyBytes, wire, pos, mac);
  memcpy(sig, mac, kSysopSigBytes);
  return true;
}

bool sameSig(const uint8_t* a, const uint8_t* b) {
  uint8_t d = 0;
  for (size_t i = 0; i < kSysopSigBytes; i++) d |= static_cast<uint8_t>(a[i] ^ b[i]);
  return d == 0;
}

Packet base(uint16_t session, const char* callsign, size_t payloadLen) {
  Packet p;
  p.mode = kModeSysop;
  p.session = session;
  p.flags = kFlagStart | kFlagEnd;
  strncpy(p.callsign, callsign, kMaxCallsign);
  p.callsign[kMaxCallsign] = 0;
  p.payloadLen = static_cast<uint8_t>(payloadLen);
  return p;
}

void put32(uint8_t* d, uint32_t v) {
  d[0] = static_cast<uint8_t>(v >> 24);
  d[1] = static_cast<uint8_t>(v >> 16);
  d[2] = static_cast<uint8_t>(v >> 8);
  d[3] = static_cast<uint8_t>(v);
}
uint32_t get32(const uint8_t* d) { return (static_cast<uint32_t>(d[0]) << 24) | (static_cast<uint32_t>(d[1]) << 16) | (static_cast<uint32_t>(d[2]) << 8) | d[3]; }

}  // namespace

Packet makeSysopCommand(uint16_t session, const char* callsign, const uint8_t key[kSysopKeyBytes], const SysopCommand& c) {
  Packet p = base(session, callsign, kSysopCommandBytes);
  p.payload[0] = c.code;
  p.payload[1] = static_cast<uint8_t>(c.param >> 8);
  p.payload[2] = static_cast<uint8_t>(c.param & 0xFF);
  put32(p.payload + 3, c.counter);
  sign(p, kSysopCommandBytes - kSysopSigBytes, key, p.payload + kSysopCommandBytes - kSysopSigBytes);
  return p;
}

Packet makeSysopReply(uint16_t session, const char* callsign, const uint8_t key[kSysopKeyBytes], const SysopReply& r) {
  Packet p = base(session, callsign, kSysopReplyBytes);
  p.payload[0] = static_cast<uint8_t>(kSysopReplyBit | (r.code & 0x7F));
  p.payload[1] = r.result;
  put32(p.payload + 2, r.counter);
  put32(p.payload + 6, r.data);
  sign(p, kSysopReplyBytes - kSysopSigBytes, key, p.payload + kSysopReplyBytes - kSysopSigBytes);
  return p;
}

bool parseSysopReply(const Packet& p, const uint8_t key[kSysopKeyBytes], SysopReply& out) {
  if (p.mode != kModeSysop || p.payloadLen != kSysopReplyBytes || !(p.payload[0] & kSysopReplyBit)) return false;
  uint8_t sig[kSysopSigBytes];
  sign(const_cast<Packet&>(p), kSysopReplyBytes - kSysopSigBytes, key, sig);
  if (!sameSig(sig, p.payload + kSysopReplyBytes - kSysopSigBytes)) return false;
  out.code = static_cast<uint8_t>(p.payload[0] & 0x7F);
  out.result = p.payload[1];
  out.counter = get32(p.payload + 2);
  out.data = get32(p.payload + 6);
  return true;
}

void SysopAuth::setKey(const uint8_t key[kSysopKeyBytes]) {
  memcpy(key_, key, kSysopKeyBytes);
  haveKey_ = true;
}

void SysopAuth::setSysop(const char* callsign) {
  strncpy(sysop_, callsign ? callsign : "", kMaxCallsign);
  sysop_[kMaxCallsign] = 0;
}

SysopAuth::Verdict SysopAuth::check(const Packet& p, SysopCommand& out) {
  if (p.mode != kModeSysop || p.payloadLen != kSysopCommandBytes || (p.payload[0] & kSysopReplyBit)) return Verdict::NotCommand;
  if (!haveKey_ || sysop_[0] == 0) {
    rejected_++;
    return Verdict::NoKey;
  }
  if (strncmp(p.callsign, sysop_, kMaxCallsign + 1) != 0) {
    rejected_++;
    return Verdict::WrongCallsign;
  }
  uint8_t sig[kSysopSigBytes];
  sign(const_cast<Packet&>(p), kSysopCommandBytes - kSysopSigBytes, key_, sig);
  if (!sameSig(sig, p.payload + kSysopCommandBytes - kSysopSigBytes)) {
    rejected_++;
    return Verdict::BadSignature;
  }
  const uint32_t counter = get32(p.payload + 3);
  if (counter <= last_) {
    rejected_++;
    return Verdict::Replay;
  }
  last_ = counter;
  out.code = p.payload[0];
  out.param = static_cast<uint16_t>((p.payload[1] << 8) | p.payload[2]);
  out.counter = counter;
  return Verdict::Ok;
}

SysopResult RepeaterControl::apply(const SysopCommand& c, uint32_t nowMs, Effects& fx, uint32_t& data) {
  fx = Effects();
  SysopResult r = kSysopOk;
  switch (c.code) {
    case kSysopStatus:
      break;
    case kSysopOff:
      off_ = true;
      timed_ = false;
      fx.forceReply = true;
      break;
    case kSysopOffMinutes:
      if (c.param == 0 || c.param > kSysopMaxOffMinutes) {
        r = kSysopBadParam;
        break;
      }
      off_ = true;
      timed_ = true;
      offUntilMs_ = nowMs + static_cast<uint32_t>(c.param) * 60000u;
      fx.forceReply = true;
      break;
    case kSysopOn:
      off_ = false;
      timed_ = false;
      contActive_ = false;
      break;
    case kSysopRestart:
      fx.restart = true;
      break;
    case kSysopIdentify:
      fx.identify = true;
      break;
    case kSysopPower:
      if (static_cast<int>(c.param) < minPower_ || static_cast<int>(c.param) > maxPower_) {
        r = kSysopBadParam;
        break;
      }
      power_ = static_cast<int>(c.param);
      fx.power = power_;
      break;
    default:
      r = kSysopUnknown;
      break;
  }
  data = statusData(nowMs);
  return r;
}

bool RepeaterControl::txAllowed(uint32_t nowMs) {
  if (off_ && timed_ && static_cast<int32_t>(nowMs - offUntilMs_) >= 0) {
    off_ = false;
    timed_ = false;
    contActive_ = false;
  }
  return !off_;
}

bool RepeaterControl::noteTx(uint32_t nowMs, uint32_t airMs, bool message) {
  if (!contActive_ || nowMs - contEndMs_ > kContinuousGapMs) {
    contActive_ = true;
    contStartMs_ = nowMs;
  }
  contEndMs_ = nowMs + airMs;
  const uint32_t limit = message ? kMaxMessageTxMs : kMaxVoiceTxMs;
  if (contEndMs_ - contStartMs_ > limit) {
    off_ = true;
    timed_ = true;
    offUntilMs_ = nowMs + kSafetyOffMs;
    contActive_ = false;
    trips_++;
    return false;
  }
  return true;
}

uint32_t RepeaterControl::statusData(uint32_t nowMs) const {
  uint32_t minutes = 0;
  if (off_) {
    if (timed_) {
      const int32_t left = static_cast<int32_t>(offUntilMs_ - nowMs);
      minutes = left > 0 ? static_cast<uint32_t>((left + 59999) / 60000) : 0;
    } else {
      minutes = 0xFFFF;
    }
  }
  return (static_cast<uint32_t>(off_ ? 1 : 0) << 24) | (static_cast<uint32_t>(power_ & 0xFF) << 16) | (minutes & 0xFFFF);
}

}  // namespace lcr
