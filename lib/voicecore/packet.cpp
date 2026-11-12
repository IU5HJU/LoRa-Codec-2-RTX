#include "packet.h"

#include <cstring>

namespace lcr {

bool isValidCallsign(const char* cs) {
  if (!cs) return false;
  const size_t n = strnlen(cs, kMaxCallsign + 1);
  if (n == 0 || n > kMaxCallsign) return false;
  for (size_t i = 0; i < n; i++) {
    const char c = cs[i];
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/';
    if (!ok) return false;
  }
  return true;
}

bool isValidText(const char* t, size_t len) {
  if (!t || len == 0 || len > kMaxTextChars) return false;
  for (size_t i = 0; i < len; i++) {
    const unsigned char c = static_cast<unsigned char>(t[i]);
    if (c < 0x20 || c > 0x7E || c == '{' || c == '}') return false;  // le graffe hanno un significato in altre reti di testo
  }
  return true;
}

Packet makeBusyPacket(uint16_t session, const char* callsign, uint16_t seconds) {
  Packet p;
  p.mode = kModeBusy;
  p.session = session;
  p.flags = kFlagStart | kFlagEnd;
  strncpy(p.callsign, callsign, kMaxCallsign);
  p.callsign[kMaxCallsign] = 0;
  p.payloadLen = kBusyPayloadBytes;
  p.payload[0] = static_cast<uint8_t>(seconds >> 8);
  p.payload[1] = static_cast<uint8_t>(seconds & 0xFF);
  return p;
}

uint16_t busySeconds(const Packet& p) {
  if (p.mode != kModeBusy || p.payloadLen != kBusyPayloadBytes) return 0;
  return static_cast<uint16_t>((p.payload[0] << 8) | p.payload[1]);
}

bool isPlaceholderCallsign(const char* cs) {
  return cs && (strncmp(cs, "N0CALL", kMaxCallsign + 1) == 0 || strncmp(cs, "NOCALL", kMaxCallsign + 1) == 0);
}

size_t encodePacket(const Packet& p, uint8_t* out, size_t cap) {
  const bool cs = p.callsign[0] != 0;
  if (cs && !isValidCallsign(p.callsign)) return 0;
  if (p.payloadLen > kMaxPayload) return 0;
  if (p.mode == kModeSysop && (!cs || (p.payloadLen != kSysopCommandBytes && p.payloadLen != kSysopReplyBytes))) return 0;
  if (p.mode == kModeBusy && (!cs || p.payloadLen != kBusyPayloadBytes)) return 0;
  if (p.mode == kModeText && (!cs || !isValidText(reinterpret_cast<const char*>(p.payload), p.payloadLen))) return 0;
  const size_t csLen = cs ? strlen(p.callsign) : 0;
  const bool hasPos = p.position.valid;
  if (hasPos && (!cs || !isValidCoordinate(p.position.latE7, p.position.lonE7))) return 0;
  const bool hasTotal = p.totalPackets != 0;
  if (hasTotal && !(p.flags & kFlagStart)) return 0;  // il totale si dichiara solo nel pacchetto START
  const size_t need = 5 + (cs ? 1 + csLen : 0) + (hasPos ? kPositionBytes : 0) + (hasTotal ? 1 : 0) + p.payloadLen;
  if (need > cap) return 0;

  const uint8_t flags = static_cast<uint8_t>((p.flags & ~(kFlagCallsign | kFlagPosition | kFlagTotal)) |
                                             (cs ? kFlagCallsign : 0) | (hasPos ? kFlagPosition : 0) |
                                             (hasTotal ? kFlagTotal : 0));
  size_t pos = 0;
  out[pos++] = static_cast<uint8_t>((kProtocolVersion << 4) | (p.mode & 0x0F));
  out[pos++] = static_cast<uint8_t>(p.session >> 8);
  out[pos++] = static_cast<uint8_t>(p.session & 0xFF);
  out[pos++] = p.seq;
  out[pos++] = flags;
  if (cs) {
    out[pos++] = static_cast<uint8_t>(csLen);
    memcpy(out + pos, p.callsign, csLen);
    pos += csLen;
  }
  if (hasPos) {
    encodePosition(p.position, out + pos);
    pos += kPositionBytes;
  }
  if (hasTotal) out[pos++] = p.totalPackets;
  if (p.payloadLen) memcpy(out + pos, p.payload, p.payloadLen);
  pos += p.payloadLen;
  return pos;
}

bool decodePacket(const uint8_t* in, size_t len, Packet& out) {
  if (!in || len < 5) return false;
  if ((in[0] >> 4) != kProtocolVersion) return false;

  Packet q;
  q.mode = in[0] & 0x0F;
  q.session = static_cast<uint16_t>((in[1] << 8) | in[2]);
  q.seq = in[3];
  q.flags = in[4];
  if (q.flags & ~kFlagsKnown) return false;
  if (q.seq >= kMaxPackets) return false;

  size_t pos = 5;
  if (q.flags & kFlagCallsign) {
    if (pos >= len) return false;
    const size_t n = in[pos++];
    if (n == 0 || n > kMaxCallsign || pos + n > len) return false;
    memcpy(q.callsign, in + pos, n);
    q.callsign[n] = 0;
    if (!isValidCallsign(q.callsign)) return false;
    pos += n;
  }
  if (q.flags & kFlagPosition) {
    if (!(q.flags & kFlagCallsign) || pos + kPositionBytes > len) return false;
    decodePosition(in + pos, q.position);
    if (!isValidCoordinate(q.position.latE7, q.position.lonE7)) return false;
    pos += kPositionBytes;
  }
  if (q.flags & kFlagTotal) {
    if (!(q.flags & kFlagStart) || pos >= len) return false;
    q.totalPackets = in[pos++];
    if (q.totalPackets == 0) return false;
  }

  const size_t pl = len - pos;
  if (pl > kMaxPayload) return false;

  if (q.flags & kFlagParity) {
    if (q.mode == kModeText || q.mode == kModeBusy || q.mode == kModeSysop) return false;
    if (q.flags & (kFlagStart | kFlagEnd | kFlagCallsign | kFlagPosition | kFlagTotal)) return false;
    if (pl < kParityHeader) return false;
    const uint8_t count = in[pos];
    const uint8_t xorLen = in[pos + 1];
    if (count == 0 || count > kParityGroup) return false;
    if (xorLen >= 16) return false;  // XOR dei conteggi di frame (ognuno <= 12)
  } else if (q.mode == kModeSysop) {
    if ((q.flags & (kFlagStart | kFlagEnd)) != (kFlagStart | kFlagEnd) || !(q.flags & kFlagCallsign)) return false;
    if (q.flags & (kFlagPosition | kFlagTotal) || (pl != kSysopCommandBytes && pl != kSysopReplyBytes)) return false;
  } else if (q.mode == kModeBusy) {
    // Annuncio di occupato: un solo pacchetto START|END con il nominativo e 2 byte di secondi.
    if ((q.flags & (kFlagStart | kFlagEnd)) != (kFlagStart | kFlagEnd) || !(q.flags & kFlagCallsign)) return false;
    if (q.flags & (kFlagPosition | kFlagTotal) || pl != kBusyPayloadBytes) return false;
  } else if (q.mode == kModeText) {
    // Messaggio di testo: un solo pacchetto START|END con il nominativo, senza posizione ne' totale.
    if ((q.flags & (kFlagStart | kFlagEnd)) != (kFlagStart | kFlagEnd) || !(q.flags & kFlagCallsign)) return false;
    if (q.flags & (kFlagPosition | kFlagTotal)) return false;
    if (!isValidText(reinterpret_cast<const char*>(in + pos), pl)) return false;
  } else {
    const size_t fb = frameBytesFor(q.mode);
    if (pl > kFramesPerPacket * fb || pl % fb != 0) return false;
  }

  q.payloadLen = static_cast<uint8_t>(pl);
  if (pl) memcpy(q.payload, in + pos, pl);
  out = q;
  return true;
}

void ParityAccumulator::reset() {
  frameBytes_ = kFrameBytes;
  count_ = 0;
  xorLen_ = 0;
  xorFlags_ = 0;
  maxLen_ = 0;
  memset(buf_, 0, sizeof(buf_));
}

void ParityAccumulator::add(const Packet& d) {
  count_++;
  frameBytes_ = frameBytesFor(d.mode);
  xorLen_ ^= static_cast<uint8_t>(d.payloadLen / frameBytes_);  // XOR dei frame per pacchetto
  xorFlags_ ^= (d.flags & kFlagsParityMask);
  if (d.payloadLen > maxLen_) maxLen_ = d.payloadLen;
  for (size_t i = 0; i < d.payloadLen; i++) buf_[i] ^= d.payload[i];
}

void ParityAccumulator::build(Packet& out, uint16_t session, uint8_t mode, uint8_t seq) const {
  out = Packet();
  out.mode = mode;
  out.session = session;
  out.seq = seq;
  out.flags = kFlagParity;
  out.payload[0] = count_;
  out.payload[1] = xorLen_;
  out.payload[2] = xorFlags_;
  memcpy(out.payload + kParityHeader, buf_, maxLen_);
  out.payloadLen = static_cast<uint8_t>(kParityHeader + maxLen_);
}

bool recoverFromParity(const Packet& parity, const Packet* const* group, size_t n, Packet& out) {
  if (!parity.isParity() || parity.payloadLen < kParityHeader) return false;
  const size_t count = parity.payload[0];
  if (count == 0 || count > kParityGroup || n != count || parity.seq < count) return false;
  const size_t xorBytes = parity.payloadLen - kParityHeader;

  size_t missingIdx = 0;
  size_t missing = 0;
  for (size_t i = 0; i < n; i++) {
    if (!group[i]) {
      missing++;
      missingIdx = i;
    }
  }
  if (missing != 1) return false;

  const size_t fb = frameBytesFor(parity.mode);
  uint8_t buf[kMaxDataPayload];
  memset(buf, 0, sizeof(buf));
  if (xorBytes > sizeof(buf)) return false;
  memcpy(buf, parity.payload + kParityHeader, xorBytes);
  uint8_t frames = parity.payload[1];
  uint8_t flags = parity.payload[2];
  for (size_t i = 0; i < n; i++) {
    if (!group[i]) continue;
    frames ^= static_cast<uint8_t>(group[i]->payloadLen / fb);
    flags ^= (group[i]->flags & kFlagsParityMask);
    for (size_t k = 0; k < group[i]->payloadLen; k++) buf[k] ^= group[i]->payload[k];
  }
  const size_t len = static_cast<size_t>(frames) * fb;
  if (frames > kFramesPerPacket || len > xorBytes) return false;

  Packet r;
  r.mode = parity.mode;
  r.session = parity.session;
  r.seq = static_cast<uint8_t>(parity.seq - count + missingIdx);
  r.flags = flags & kFlagsParityMask;
  r.payloadLen = static_cast<uint8_t>(len);
  memcpy(r.payload, buf, len);
  out = r;
  return true;
}

}  // namespace lcr
