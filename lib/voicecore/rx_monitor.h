// Monitor audio della scheda RX del ripetitore: con un altoparlante collegato si ascolta il traffico voce che la RX riceve
// sulla frequenza di ingresso (non la trasmissione della TX). E' la sola parte di ricezione del nodo normale: riassembla i
// pacchetti, recupera i persi con la parita' e riproduce i frame al ritmo di 40 ms. Non trasmette mai.
// Solo la voce del modo del profilo (il testo e gli altri modi non si ascoltano). Nel profilo Messaggio la voce si ascolta a
// messaggio concluso, come dopo una ricezione normale prima della casella dei messaggi.
// Indipendente dalla scheda e testato sul PC (tests/test_rx_monitor.cpp).
#pragma once

#include <cstdint>

#include "interfaces.h"
#include "profile.h"
#include "protocol.h"
#include "rx_stream.h"

namespace lcr {

class RxMonitor {
 public:
  RxMonitor(uint8_t profileId, ICodec& codec, IAudioOut& out);

  // Pacchetto decodificato dalla radio. true se e' voce del modo del profilo e appartiene alla ricezione in corso (o ne
  // avvia una).
  bool onPacket(const Packet& p, uint32_t nowMs);
  // Da chiamare spesso (almeno ogni ~10 ms): riproduce un frame ogni kFrameMs.
  void tick(uint32_t nowMs);

  bool active() const { return playing_; }
  const char* callsign() const { return rx_.callsign(); }
  uint32_t sessionsHeard() const { return sessions_; }
  uint32_t framesPlayed() const { return framesPlayed_; }
  uint32_t framesLost() const { return framesLost_; }

 private:
  const Profile& prof_;
  ICodec& codec_;
  IAudioOut& out_;
  RxStream rx_;
  bool playing_ = false;
  uint32_t nextPlayMs_ = 0;
  uint32_t sessions_ = 0, framesPlayed_ = 0, framesLost_ = 0;
};

}  // namespace lcr
