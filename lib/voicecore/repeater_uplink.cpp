#include "repeater_uplink.h"

#include "protocol.h"

namespace lcr {

RepeaterUplink::RepeaterUplink(INodeNet& net, const NodeLink::Config& link, uint32_t bootId, uint32_t (*rng)(), uint8_t voiceMode)
    : net_(net), link_(link, bootId, rng), voiceMode_(voiceMode) {}

// Voce: chi arriva per primo prende l'uscita; l'altro e' scartato finche' il primo non finisce o tace per kHangMs. La parita' dopo la fine non
// prende l'uscita. who: 1 radio, 2 rete.
bool RepeaterUplink::takeVoice(int who, const Packet& p, uint32_t nowMs) {
  const bool hanging = owner_ != 0 && nowMs - ownerMs_ <= kHangMs;
  if (hanging && owner_ != who) return false;
  if (p.isParity()) {
    if (hanging) ownerMs_ = nowMs;
    return true;
  }
  owner_ = who;
  ownerMs_ = nowMs;
  if (p.flags & kFlagEnd) owner_ = 0;  // fine della trasmissione: l'uscita e' libera
  return true;
}

bool RepeaterUplink::fromRadio(const Packet& p, uint32_t nowMs) {
  const bool voice = p.mode == voiceMode_;
  if (voice && !takeVoice(1, p, nowMs)) {
    stats_.localBlocked++;
    return false;
  }
  if (!link_.linkUp(nowMs)) {
    stats_.droppedLinkDown++;
    return true;
  }
  uint8_t wire[kMaxWireBytes], dg[kNodeMaxDatagram];
  const size_t m = encodePacket(p, wire, sizeof(wire));
  const size_t k = m ? link_.makePacket(wire, m, dg, sizeof(dg)) : 0;
  if (k) {
    net_.sendDatagram(dg, k);
    stats_.toNet++;
  }
  return true;
}

bool RepeaterUplink::onDatagram(const uint8_t* d, size_t n, uint32_t nowMs, uint8_t* wire, size_t cap, size_t& len) {
  len = 0;
  NodeLink::Rx& rx = link_.onDatagram(d, n, nowMs);
  for (size_t i = 0; i < rx.nReplies; i++) net_.sendDatagram(rx.replies[i].data, rx.replies[i].len);
  if (rx.kind != NodeLink::Kind::Packet) return false;
  Packet p;
  if (!decodePacket(rx.packet, rx.packetLen, p) || !(p.mode == voiceMode_ || p.mode == kModeText)) {
    stats_.netBad++;
    return false;
  }
  if (p.mode == voiceMode_ && !takeVoice(2, p, nowMs)) {
    stats_.netBlocked++;
    return false;
  }
  p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);  // un pacchetto di rete e' gia' ripetuto: la TX lo trasmette e basta
  const size_t m = encodePacket(p, wire, cap);
  if (m == 0) {
    stats_.netBad++;
    return false;
  }
  len = m;
  stats_.fromNet++;
  return true;
}

void RepeaterUplink::tick(uint32_t nowMs) {
  uint8_t dg[kNodeMaxDatagram];
  const size_t m = link_.maintenance(nowMs, dg, sizeof(dg));
  if (m) net_.sendDatagram(dg, m);
  if (owner_ != 0 && nowMs - ownerMs_ > kHangMs) owner_ = 0;
}

}  // namespace lcr
