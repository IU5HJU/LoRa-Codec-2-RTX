// Il collegamento in rete della scheda RX del ripetitore: un reflector finto (un NodeLink) dall'altra parte.
#include <cstring>
#include <deque>
#include <vector>

#include "node_link.h"
#include "packet.h"
#include "protocol.h"
#include "repeater_uplink.h"
#include "testing.h"

using namespace lcr;

static uint32_t g_rng = 99;
static uint32_t rngFn() { return g_rng += 6151; }
typedef std::vector<uint8_t> Bytes;

struct Net : public INodeNet {
  std::deque<Bytes> out;
  void sendDatagram(const uint8_t* d, size_t n) override { out.push_back(Bytes(d, d + n)); }
};

struct Rig {
  Net net;
  RepeaterUplink up;
  NodeLink refl;  // il reflector (un solo client)
  std::vector<Packet> atReflector;
  uint32_t t = 1000;
  static NodeLink::Config cfg(uint32_t id, uint32_t peer) {
    NodeLink::Config c;
    c.nodeId = id;
    c.peerId = peer;
    memset(c.key, 0x5A, sizeof(c.key));
    return c;
  }
  explicit Rig(uint8_t mode = kModeCodec2_3200) : up(net, cfg(7, 1000), 0x1111, rngFn, mode), refl(cfg(1000, 7), 0x2222, rngFn) {}
  // Scambia i datagrammi fra il ripetitore e il reflector finto
  void pump() {
    for (int round = 0; round < 10; round++) {
      bool any = false;
      while (!net.out.empty()) {
        any = true;
        const Bytes d = net.out.front();
        net.out.pop_front();
        NodeLink::Rx& rx = refl.onDatagram(d.data(), d.size(), t);
        for (size_t i = 0; i < rx.nReplies; i++) toUplink.push_back(Bytes(rx.replies[i].data, rx.replies[i].data + rx.replies[i].len));
        if (rx.kind == NodeLink::Kind::Packet) {
          Packet p;
          if (decodePacket(rx.packet, rx.packetLen, p)) atReflector.push_back(p);
        }
      }
      while (!toUplink.empty()) {
        any = true;
        const Bytes d = toUplink.front();
        toUplink.pop_front();
        uint8_t wire[kMaxWireBytes];
        size_t len = 0;
        if (up.onDatagram(d.data(), d.size(), t, wire, sizeof(wire), len)) delivered.push_back(Bytes(wire, wire + len));
      }
      if (!any) break;
    }
  }
  void step() {
    up.tick(t);
    uint8_t dg[kNodeMaxDatagram];
    const size_t m = refl.maintenance(t, dg, sizeof(dg));
    if (m) toUplink.push_back(Bytes(dg, dg + m));
    pump();
  }
  void run(uint32_t ms) {
    const uint32_t end = t + ms;
    while (t < end) {
      step();
      t += 100;
    }
  }
  // Un pacchetto dal reflector verso il ripetitore
  void fromReflector(const Packet& p) {
    uint8_t wire[kMaxWireBytes], dg[kNodeMaxDatagram];
    const size_t w = encodePacket(p, wire, sizeof(wire));
    const size_t k = refl.makePacket(wire, w, dg, sizeof(dg));
    toUplink.push_back(Bytes(dg, dg + k));
    pump();
  }
  std::deque<Bytes> toUplink;
  std::vector<Bytes> delivered;  // pacchetti consegnati alla TX
};

static Packet voice(uint8_t mode, uint16_t session, uint8_t seq, uint8_t flags, const char* call = "IU5HJU") {
  Packet p;
  p.mode = mode;
  p.session = session;
  p.seq = seq;
  p.flags = flags;
  if (flags & kFlagStart) strncpy(p.callsign, call, kMaxCallsign);
  p.payloadLen = static_cast<uint8_t>(frameBytesFor(mode) * 2);
  for (size_t i = 0; i < p.payloadLen; i++) p.payload[i] = static_cast<uint8_t>(seq + i);
  return p;
}

static Packet text(const char* call, const char* t) {
  Packet p;
  p.mode = kModeText;
  p.session = 0x4321;
  p.flags = kFlagStart | kFlagEnd;
  strncpy(p.callsign, call, kMaxCallsign);
  p.payloadLen = static_cast<uint8_t>(strlen(t));
  memcpy(p.payload, t, p.payloadLen);
  return p;
}

static Packet decoded(const Bytes& b) {
  Packet p;
  CHECK(decodePacket(b.data(), b.size(), p));
  return p;
}

static void testLinkAndRadioToNet() {
  Rig r;
  // collegamento non ancora attivo: la voce va alla TX ma non in rete (contata)
  CHECK(!r.up.linkUp(r.t));
  CHECK(r.up.fromRadio(voice(kModeCodec2_3200, 0x0A, 0, kFlagStart), r.t));
  CHECK(r.up.stats().droppedLinkDown == 1 && r.up.stats().toNet == 0 && r.atReflector.empty());
  r.run(4000);
  CHECK(r.up.linkUp(r.t) && r.refl.linkUp(r.t));
  // ora il testo e la voce partono verso il reflector
  CHECK(r.up.fromRadio(text("IU5HJU", "ciao"), r.t));
  r.pump();
  CHECK(r.atReflector.size() == 1 && r.atReflector[0].mode == kModeText && strcmp(r.atReflector[0].callsign, "IU5HJU") == 0);
  r.t += 5000;
  CHECK(r.up.fromRadio(voice(kModeCodec2_3200, 0x0B, 0, kFlagStart), r.t));
  CHECK(r.up.fromRadio(voice(kModeCodec2_3200, 0x0B, 1, kFlagEnd), r.t));
  r.pump();
  CHECK(r.atReflector.size() == 3 && r.up.stats().toNet == 3);
}

static void testNetToRadio() {
  Rig r;
  r.run(4000);
  r.fromReflector(voice(kModeCodec2_3200, 0x0C, 0, kFlagStart, "IZ0ABC"));
  CHECK(r.delivered.size() == 1);
  const Packet p = decoded(r.delivered[0]);
  CHECK(p.mode == kModeCodec2_3200 && (p.flags & kFlagRelayed) && strcmp(p.callsign, "IZ0ABC") == 0 && p.payloadLen == 32);
  CHECK(r.up.stats().fromNet == 1 && r.up.owner(r.t) == 2);
  r.fromReflector(text("IZ0ABC", "testo di rete"));
  CHECK(r.delivered.size() == 2 && decoded(r.delivered[1]).mode == kModeText);
  // un modo che il ripetitore non ripete (1200 con il ripetitore a 3200, il sysop, la prova) si scarta
  r.fromReflector(voice(kModeCodec2_1200, 0x0D, 0, kFlagStart));
  CHECK(r.delivered.size() == 2 && r.up.stats().netBad == 1);
}

static void testFirstComeFirstServed() {
  Rig r;
  r.run(4000);
  // parla la rete: la voce locale si scarta e non va in rete
  r.fromReflector(voice(kModeCodec2_3200, 0x11, 0, kFlagStart));
  CHECK(r.up.owner(r.t) == 2);
  const size_t atRefl = r.atReflector.size();
  CHECK(!r.up.fromRadio(voice(kModeCodec2_3200, 0x22, 0, kFlagStart), r.t));
  r.pump();
  CHECK(r.up.stats().localBlocked == 1 && r.atReflector.size() == atRefl);
  // il testo locale passa sempre
  CHECK(r.up.fromRadio(text("IU5HJU", "ci sono"), r.t));
  // la rete finisce: l'uscita e' libera e la voce locale passa
  r.fromReflector(voice(kModeCodec2_3200, 0x11, 1, kFlagEnd));
  CHECK(r.up.owner(r.t) == 0);
  CHECK(r.up.fromRadio(voice(kModeCodec2_3200, 0x22, 0, kFlagStart), r.t) && r.up.owner(r.t) == 1);
  // ora parla la radio: la voce dalla rete si scarta (e non arriva alla TX)
  const size_t deliv = r.delivered.size();
  r.fromReflector(voice(kModeCodec2_3200, 0x33, 0, kFlagStart));
  CHECK(r.delivered.size() == deliv && r.up.stats().netBlocked == 1);
  r.fromReflector(text("IZ0ABC", "dalla rete"));
  CHECK(r.delivered.size() == deliv + 1);
  // chi parla tace senza pacchetto di fine: dopo kHangMs si libera
  r.t += RepeaterUplink::kHangMs + 100;
  r.up.tick(r.t);
  CHECK(r.up.owner(r.t) == 0);
  r.fromReflector(voice(kModeCodec2_3200, 0x33, 0, kFlagStart));
  CHECK(r.delivered.size() == deliv + 2);
  // la parita' dopo la fine non prende l'uscita
  r.fromReflector(voice(kModeCodec2_3200, 0x33, 1, kFlagEnd));
  CHECK(r.up.owner(r.t) == 0);
  Packet par;
  par.mode = kModeCodec2_3200;
  par.session = 0x33;
  par.seq = 2;
  par.flags = kFlagParity;
  par.payloadLen = static_cast<uint8_t>(kParityHeader + 16);
  par.payload[0] = 2;
  r.fromReflector(par);
  CHECK(r.up.owner(r.t) == 0 && r.delivered.size() == deliv + 4);
}

static void testLinkDownDrops() {
  Rig r;
  r.run(4000);
  // il reflector sparisce: dopo kNodeDeadMs il collegamento e' giu', la voce locale va solo alla TX
  r.t += kNodeDeadMs + 1000;
  CHECK(!r.up.linkUp(r.t));
  const size_t n = r.atReflector.size();
  CHECK(r.up.fromRadio(voice(kModeCodec2_3200, 0x44, 0, kFlagStart), r.t));
  r.pump();
  CHECK(r.atReflector.size() == n && r.up.stats().droppedLinkDown >= 1);
}

int main() {
  testLinkAndRadioToNet();
  testNetToRadio();
  testFirstComeFirstServed();
  testLinkDownDrops();
  TEST_MAIN_END();
}
