// Il collegamento in rete della scheda RX del ripetitore (docs/studio-reflector.md): un NodeLink verso il reflector (o verso un nodo) e le regole per
// non mescolare la voce che arriva dalla radio con quella che arriva dalla rete, perche' la TX ritrasmette tutto sulla stessa uscita.
//   radio -> rete: quello che il ripetitore ha deciso di ripetere (Repeater) va anche al reflector, se il collegamento e' attivo;
//   rete -> radio: i pacchetti del reflector (il modo voce del ripetitore e il testo) vanno alla TX, con il flag RELAYED, per essere trasmessi.
// Chi parla per primo tiene l'uscita: finche' arriva voce dalla radio (o dalla rete) la voce dell'altra parte si scarta, fino al pacchetto di fine o
// a kHangMs senza pacchetti. Il testo passa sempre. Solo logica, senza Wi-Fi ne' orologio (tests/test_repeater_uplink.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "node_core.h"
#include "node_link.h"
#include "packet.h"

namespace lcr {

class RepeaterUplink {
 public:
  static constexpr uint32_t kHangMs = 3000;

  struct Stats {
    uint32_t toNet = 0;          // pacchetti mandati al reflector
    uint32_t fromNet = 0;        // pacchetti dal reflector consegnati alla TX
    uint32_t localBlocked = 0;   // voce dalla radio scartata perche' parlava la rete
    uint32_t netBlocked = 0;     // voce dalla rete scartata perche' parlava la radio
    uint32_t droppedLinkDown = 0;  // da mandare al reflector ma collegamento non attivo
    uint32_t netBad = 0;         // pacchetti dalla rete non validi o di un modo non ripetuto
  };

  RepeaterUplink(INodeNet& net, const NodeLink::Config& link, uint32_t bootId, uint32_t (*rng)(), uint8_t voiceMode);

  // Un pacchetto che il ripetitore ripeterebbe (gia' deciso da Repeater): true se va alla TX, false se la rete sta parlando (voce scartata).
  // Se va alla TX e il collegamento e' attivo parte anche verso il reflector.
  bool fromRadio(const Packet& p, uint32_t nowMs);
  // Un datagramma dalla rete: risposte della sfida spedite da qui; se c'e' un pacchetto da trasmettere ritorna true e lo mette in wire/len.
  bool onDatagram(const uint8_t* d, size_t n, uint32_t nowMs, uint8_t* wire, size_t cap, size_t& len);
  // HELLO e KEEPALIVE.
  void tick(uint32_t nowMs);

  bool linkUp(uint32_t nowMs) const { return link_.linkUp(nowMs); }
  void setPublicAddress(uint32_t ipv4) { link_.setPublicAddress(ipv4); }
  const Stats& stats() const { return stats_; }
  const NodeLink& link() const { return link_; }
  // Chi tiene l'uscita adesso: 0 nessuno, 1 la radio, 2 la rete.
  int owner(uint32_t nowMs) const { return owner_ != 0 && nowMs - ownerMs_ <= kHangMs ? owner_ : 0; }

 private:
  bool takeVoice(int who, const Packet& p, uint32_t nowMs);

  INodeNet& net_;
  NodeLink link_;
  uint8_t voiceMode_;
  int owner_ = 0;
  uint32_t ownerMs_ = 0;
  Stats stats_;
};

}  // namespace lcr
