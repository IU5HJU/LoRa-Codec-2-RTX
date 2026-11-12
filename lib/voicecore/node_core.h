// Il nodo VoLoRa su Wi-Fi (docs/studio-nodo-internet.md): una sola radio e un collegamento UDP con un altro nodo.
//   radio -> rete: un pacchetto valido ricevuto in radio passa dalle regole del ripetitore (Repeater: modo voluto, non gia' ripetuto, non
//                  duplicato, sessione non troppo lunga) e, se il collegamento e' attivo, parte verso l'altro nodo con il flag RELAYED;
//   rete -> radio: un datagramma firmato con un pacchetto valido va in coda e si trasmette (con controllo del canale, CAD) con il flag RELAYED.
// Priorita' alla voce locale: se il canale e' occupato il pacchetto di rete aspetta in coda; a coda piena i pacchetti nuovi si perdono (contati).
// Qui c'e' solo la logica, senza Wi-Fi ne' orologio: provata sul PC (tests/test_node_core.cpp). Solo profilo Standard (modo 1) e testo (modo 7).
#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces.h"
#include "node_link.h"
#include "repeater.h"
#include "repeater_link.h"

namespace lcr {

class INodeNet {
 public:
  virtual ~INodeNet() {}
  virtual void sendDatagram(const uint8_t* data, size_t len) = 0;  // verso l'altro nodo
};

class NodeCore {
 public:
  struct Stats {
    uint32_t radioRx = 0;         // pacchetti del progetto ricevuti in radio
    uint32_t radioBad = 0;        // ricevuti ma non del progetto
    uint32_t toNet = 0;           // pacchetti mandati all'altro nodo
    uint32_t droppedLinkDown = 0; // da ripetere ma collegamento non attivo
    uint32_t fromNet = 0;         // pacchetti arrivati dalla rete e messi in coda
    uint32_t netBad = 0;          // datagrammi validi con un pacchetto non accettabile
    uint32_t transmitted = 0;     // pacchetti trasmessi in radio
    uint32_t busyDeferred = 0;    // trasmissioni rimandate perche' il canale era occupato
    uint32_t discardedNoTx = 0;   // scartati perche' il nominativo non permette di trasmettere
  };

  NodeCore(IRadio& radio, INodeNet& net, const NodeLink::Config& link, uint32_t bootId, uint32_t (*rng)());

  // Un datagramma dall'altro nodo.
  void onDatagram(const uint8_t* d, size_t n, uint32_t nowMs);
  // Un giro: manutenzione del collegamento, ricezione radio, trasmissione di un pacchetto. mayTransmit false (N0CALL): in radio non esce niente.
  void tick(uint32_t nowMs, bool mayTransmit);

  bool linkUp(uint32_t nowMs) const { return link_.linkUp(nowMs); }
  const NodeLink& link() const { return link_; }
  const TxQueue& queue() const { return queue_; }
  const Stats& stats() const { return stats_; }
  const Repeater& repeater() const { return rep_; }

 private:
  IRadio& radio_;
  INodeNet& net_;
  NodeLink link_;
  Repeater rep_;
  TxQueue queue_;
  Stats stats_;
};

}  // namespace lcr
