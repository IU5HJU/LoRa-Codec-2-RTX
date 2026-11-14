#include "node_link.h"

#include <cstring>

#include "sha256.h"

namespace lcr {

namespace {

void put32(uint8_t* d, uint32_t v) {
  d[0] = static_cast<uint8_t>(v >> 24);
  d[1] = static_cast<uint8_t>(v >> 16);
  d[2] = static_cast<uint8_t>(v >> 8);
  d[3] = static_cast<uint8_t>(v);
}
uint32_t get32(const uint8_t* d) { return (static_cast<uint32_t>(d[0]) << 24) | (static_cast<uint32_t>(d[1]) << 16) | (static_cast<uint32_t>(d[2]) << 8) | d[3]; }

void sign(const uint8_t key[kNodeKeyBytes], const uint8_t* msg, size_t n, uint8_t sig[kNodeSigBytes]) {
  uint8_t mac[kSha256Bytes];
  hmacSha256(key, kNodeKeyBytes, msg, n, mac);
  memcpy(sig, mac, kNodeSigBytes);
}

bool sameSig(const uint8_t* a, const uint8_t* b) {
  uint8_t d = 0;
  for (size_t i = 0; i < kNodeSigBytes; i++) d |= static_cast<uint8_t>(a[i] ^ b[i]);
  return d == 0;
}

}  // namespace

NodeLink::NodeLink(const Config& cfg, uint32_t bootId, uint32_t (*rng)()) : cfg_(cfg), boot_(bootId), rng_(rng) {}

size_t NodeLink::build(uint8_t type, const uint8_t* payload, size_t len, uint8_t* out, size_t cap) {
  if (len > kNodeMaxPayload || cap < kNodeHeaderBytes + len + kNodeSigBytes) return 0;
  out[0] = 'V';
  out[1] = 'L';
  out[2] = kNodeVersion;
  out[3] = type;
  out[4] = cfg_.channel;
  put32(out + 5, cfg_.nodeId);
  put32(out + 9, boot_);
  put32(out + 13, ++counter_);
  out[17] = static_cast<uint8_t>(len);
  if (len) memcpy(out + kNodeHeaderBytes, payload, len);
  sign(cfg_.key, out, kNodeHeaderBytes + len, out + kNodeHeaderBytes + len);
  stats_.sent++;
  return kNodeHeaderBytes + len + kNodeSigBytes;
}

// HELLO e KEEPALIVE portano lo stato dell'autenticazione: [1 se abbiamo autenticato l'altro][il suo id di avvio che abbiamo autenticato].
size_t NodeLink::makeStatus(uint8_t type, uint8_t* out, size_t cap) {
  uint8_t p[9];
  p[0] = static_cast<uint8_t>((peerAuth_ ? 1 : 0) | ((peerAuth_ && peerAcceptsMe_) ? 2 : 0));
  put32(p + 1, peerAuth_ ? peerBoot_ : 0);
  if (publicAddr_ != 0) put32(p + 5, publicAddr_);
  return build(type, p, publicAddr_ != 0 ? 9 : 5, out, cap);
}

size_t NodeLink::makeHello(uint8_t* out, size_t cap) { return makeStatus(kNodeHello, out, cap); }
size_t NodeLink::makeKeepalive(uint8_t* out, size_t cap) { return makeStatus(kNodeKeepalive, out, cap); }
size_t NodeLink::makePacket(const uint8_t* wire, size_t len, uint8_t* out, size_t cap) { return build(kNodePacket, wire, len, out, cap); }

size_t NodeLink::maintenance(uint32_t nowMs, uint8_t* out, size_t cap) {
  const uint32_t every = linkUp(nowMs) ? kNodeKeepaliveMs : kNodeHelloMs;
  if (haveSent_ && nowMs - lastSendMs_ < every) return 0;
  haveSent_ = true;
  lastSendMs_ = nowMs;
  return linkUp(nowMs) ? makeKeepalive(out, cap) : makeHello(out, cap);
}

void NodeLink::makeChallenge(uint32_t nowMs, Reply& r) {
  for (size_t i = 0; i < kNodeNonceBytes; i += 4) put32(nonce_ + i, rng_());
  challengePending_ = true;
  challengeMs_ = nowMs;
  haveChallengeMs_ = true;
  lastChallengeMs_ = nowMs;
  stats_.challenges++;
  r.len = build(kNodeChallenge, nonce_, kNodeNonceBytes, r.data, sizeof(r.data));
}

NodeLink::Rx& NodeLink::onDatagram(const uint8_t* d, size_t n, uint32_t nowMs) {
  Rx& rx = rx_;
  rx.kind = Kind::Ignored;
  rx.packet = nullptr;
  rx.packetLen = 0;
  rx.nReplies = 0;
  if (!d || n < kNodeHeaderBytes + kNodeSigBytes || d[0] != 'V' || d[1] != 'L' || d[2] != kNodeVersion) {
    stats_.ignored++;
    return rx;
  }
  const size_t len = d[17];
  if (n != kNodeHeaderBytes + len + kNodeSigBytes) {
    stats_.ignored++;
    return rx;
  }
  uint8_t sig[kNodeSigBytes];
  sign(cfg_.key, d, kNodeHeaderBytes + len, sig);
  if (!sameSig(sig, d + kNodeHeaderBytes + len)) {
    stats_.badSignature++;
    rx.kind = Kind::BadSignature;
    return rx;
  }
  const uint8_t type = d[3];
  const uint8_t channel = d[4];
  const uint32_t id = get32(d + 5), boot = get32(d + 9), counter = get32(d + 13);
  const uint8_t* payload = d + kNodeHeaderBytes;
  if (channel != cfg_.channel || id == cfg_.nodeId || (cfg_.peerId != 0 && id != cfg_.peerId)) {  // altro canale, noi stessi, o un nodo che non e' il nostro
    stats_.ignored++;
    return rx;
  }

  // Una sfida va sempre raccolta: si risponde con lo stesso contenuto (firmato, quindi fresco). Se non conosciamo ancora il nodo, gli mandiamo la nostra.
  if (type == kNodeChallenge) {
    if (len != kNodeNonceBytes) {
      stats_.ignored++;
      return rx;
    }
    rx.replies[rx.nReplies].len = build(kNodeResponse, payload, len, rx.replies[rx.nReplies].data, sizeof(rx.replies[0].data));
    rx.nReplies++;
    const bool known = peerAuth_ && boot == peerBoot_ && id == peerSeenId_;
    if (!known && (!haveChallengeMs_ || nowMs - lastChallengeMs_ >= kNodeChallengeGapMs)) {
      makeChallenge(nowMs, rx.replies[rx.nReplies]);
      rx.nReplies++;
    }
    rx.kind = Kind::Control;
    stats_.received++;
    return rx;
  }

  // La risposta alla nostra sfida: autentica il nodo con questo id di avvio.
  if (type == kNodeResponse) {
    if (challengePending_ && len == kNodeNonceBytes && memcmp(payload, nonce_, kNodeNonceBytes) == 0 && nowMs - challengeMs_ < kNodeChallengeLifeMs) {
      challengePending_ = false;
      peerAuth_ = true;
      peerSeenId_ = id;
      peerBoot_ = boot;
      peerLastCounter_ = counter;
      peerAcceptsMe_ = false;  // finche' l'altro non dice di aver autenticato noi
      peerPublic_ = 0;         // un nuovo avvio: l'indirizzo dichiarato prima non vale piu'
      haveRx_ = true;
      lastRxMs_ = nowMs;
      stats_.authenticated++;
      stats_.received++;
      rx.kind = Kind::Control;
      rx.replies[0].len = makeStatus(kNodeKeepalive, rx.replies[0].data, sizeof(rx.replies[0].data));  // subito: l'altro sa che l'abbiamo accettato
      rx.nReplies = 1;
    } else {
      stats_.ignored++;  // risposta senza sfida o scaduta
    }
    return rx;
  }

  if (!(peerAuth_ && boot == peerBoot_ && id == peerSeenId_)) {  // nodo non ancora autenticato, o riavviato: si sfida, senza accettare niente
    stats_.unknown++;
    rx.kind = Kind::Unknown;
    if (!haveChallengeMs_ || nowMs - lastChallengeMs_ >= kNodeChallengeGapMs) {
      makeChallenge(nowMs, rx.replies[0]);
      rx.nReplies = 1;
    }
    return rx;
  }
  if (counter <= peerLastCounter_) {
    stats_.replays++;
    rx.kind = Kind::Replay;
    return rx;
  }
  peerLastCounter_ = counter;
  haveRx_ = true;
  lastRxMs_ = nowMs;
  stats_.received++;
  if (type == kNodePacket) {
    rx.kind = Kind::Packet;
    rx.packet = payload;
    rx.packetLen = len;
  } else {
    rx.kind = Kind::Control;  // HELLO o KEEPALIVE dello stesso avvio
    if ((type == kNodeHello || type == kNodeKeepalive) && len >= 5) {
      peerAcceptsMe_ = (payload[0] & 1) != 0 && get32(payload + 1) == boot_;
      if (len >= 9) peerPublic_ = get32(payload + 5);  // l'indirizzo pubblico dichiarato dal nodo (le versioni vecchie mandano 5 byte)
      // l'altro nodo non ha ancora saputo di essere stato accettato (bit 1): glielo diciamo subito, al massimo una volta al secondo
      if (!(payload[0] & 2) && (!haveStatusReplyMs_ || nowMs - lastStatusReplyMs_ >= kNodeChallengeGapMs)) {
        haveStatusReplyMs_ = true;
        lastStatusReplyMs_ = nowMs;
        rx.replies[0].len = makeStatus(kNodeKeepalive, rx.replies[0].data, sizeof(rx.replies[0].data));
        rx.nReplies = 1;
      }
    }
  }
  return rx;
}

}  // namespace lcr
