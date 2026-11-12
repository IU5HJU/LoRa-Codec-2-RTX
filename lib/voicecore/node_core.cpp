#include "node_core.h"

#include "packet.h"
#include "protocol.h"

namespace lcr {

NodeCore::NodeCore(IRadio& radio, INodeNet& net, const NodeLink::Config& link, uint32_t bootId, uint32_t (*rng)())
    : radio_(radio), net_(net), link_(link, bootId, rng) {
  rep_.setModeMask(static_cast<uint16_t>((1u << kModeCodec2_1200) | (1u << kModeText)));  // solo profilo Standard e testo
}

void NodeCore::onDatagram(const uint8_t* d, size_t n, uint32_t nowMs) {
  NodeLink::Rx& rx = link_.onDatagram(d, n, nowMs);
  for (size_t i = 0; i < rx.nReplies; i++) net_.sendDatagram(rx.replies[i].data, rx.replies[i].len);
  if (rx.kind != NodeLink::Kind::Packet) return;
  Packet p;
  if (!decodePacket(rx.packet, rx.packetLen, p) || !(p.mode == kModeCodec2_1200 || p.mode == kModeText)) {
    stats_.netBad++;
    return;
  }
  p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);  // anche se l'altro nodo non l'avesse messo: un pacchetto di rete e' gia' ripetuto
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(p, wire, sizeof(wire));
  if (m == 0) {
    stats_.netBad++;
    return;
  }
  if (queue_.push(wire, m)) stats_.fromNet++;  // coda piena: contato dalla coda
}

void NodeCore::tick(uint32_t nowMs, bool mayTransmit) {
  uint8_t dg[kNodeMaxDatagram];
  const size_t md = link_.maintenance(nowMs, dg, sizeof(dg));
  if (md) net_.sendDatagram(dg, md);

  // radio -> rete
  uint8_t buf[kMaxRxBytes];
  for (int i = 0; i < 4; i++) {
    int rssi = 0;
    const size_t n = radio_.receive(buf, sizeof(buf), &rssi);
    if (n == 0) break;
    Packet p;
    if (!decodePacket(buf, n, p)) {
      stats_.radioBad++;
      continue;
    }
    stats_.radioRx++;
    Packet out;
    if (rep_.onPacket(p, nowMs, out) != Repeater::Verdict::Forward) continue;
    if (!link_.linkUp(nowMs)) {
      stats_.droppedLinkDown++;
      continue;
    }
    uint8_t wire[kMaxWireBytes];
    const size_t m = encodePacket(out, wire, sizeof(wire));
    const size_t k = m ? link_.makePacket(wire, m, dg, sizeof(dg)) : 0;
    if (k) {
      net_.sendDatagram(dg, k);
      stats_.toNet++;
    }
  }

  // rete -> radio
  if (queue_.size() == 0) return;
  uint8_t pkt[kMaxWireBytes];
  size_t len = 0;
  if (!mayTransmit) {
    if (queue_.next(nowMs, pkt, sizeof(pkt), len)) {
      queue_.sent(nowMs);  // senza nominativo si scarta tutto
      stats_.discardedNoTx++;
    }
    return;
  }
  if (radio_.channelBusy()) {  // una stazione locale sta trasmettendo: ha la precedenza
    stats_.busyDeferred++;
    return;
  }
  if (queue_.next(nowMs, pkt, sizeof(pkt), len)) {
    radio_.send(pkt, len);  // bloccante: dura il tempo in aria del pacchetto
    queue_.sent(nowMs);
    stats_.transmitted++;
  }
}

}  // namespace lcr
