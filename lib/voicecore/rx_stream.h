// Lato ricezione: riassembla i pacchetti di una trasmissione, recupera i
// pacchetti persi con la parita' e fornisce i frame Codec2 in ordine.
#pragma once

#include <cstddef>
#include <cstdint>

#include "packet.h"
#include "protocol.h"

namespace lcr {

class RxStream {
 public:
  enum class Kind {
    Wait,   // nessun frame disponibile ora (pre-buffer o attesa pacchetti)
    Frame,  // frame valido in Out::frame (kFrameBytes)
    Lost,   // frame perso e non recuperabile: mascherare/silenzio
    End,    // trasmissione terminata
  };
  struct Out {
    Kind kind;
    const uint8_t* frame;  // valido solo per Kind::Frame, fino alla chiamata successiva
  };

  // Tempi di attesa della ricezione: di solito quelli di protocol.h; il profilo Messaggio aspetta la fine del messaggio.
  struct Config {
    size_t prebufferFrames = kPrebufferFrames;           // frame da accumulare prima di riprodurre
    uint32_t prebufferTimeoutMs = kPrebufferTimeoutMs;   // attesa massima per il pre-buffer
    uint32_t sessionTimeoutMs = kSessionTimeoutMs;       // silenzio dopo cui la trasmissione e' chiusa
  };

  RxStream() { reset(); }
  void configure(const Config& c) { cfg_ = c; }
  void reset();

  // Ritorna true se il pacchetto appartiene alla trasmissione in corso (o ne
  // avvia una nuova). Pacchetti di un'altra trasmissione mentre una e' in
  // corso, o ritardatari di una trasmissione conclusa, ritornano false.
  bool onPacket(const Packet& p, uint32_t nowMs);

  // Da chiamare ogni kFrameMs una volta avviata la ricezione.
  Out nextFrame(uint32_t nowMs);

  bool active() const { return phase_ != Phase::Idle && phase_ != Phase::Done; }
  bool finished() const { return phase_ == Phase::Done; }
  const char* callsign() const { return callsign_; }
  // Posizione ricevuta nel pacchetto START (valid = false se non e' arrivata).
  const Position& position() const { return position_; }
  uint16_t session() const { return session_; }
  uint32_t recoveredPackets() const { return recovered_; }
  uint32_t lostFrames() const { return lostFrames_; }

 private:
  enum class Phase { Idle, Buffering, Playing, Done };
  struct Slot {
    bool have;
    bool parityDone;
    Packet pkt;
  };

  bool isParitySeq(size_t seq) const { return fec_ && (seq % kParityPeriod) == kParityGroup; }
  size_t paritySeqFor(size_t seq) const;
  bool resolved(size_t seq) const;
  size_t bufferedFrames() const;
  void tryRecoverAll();
  void finish();

  Config cfg_;
  Phase phase_;
  uint16_t session_;
  bool fec_;
  char callsign_[kMaxCallsign + 1];
  Position position_;
  uint32_t firstRxMs_;
  uint32_t lastRxMs_;
  int endSeq_;
  size_t highest_;
  size_t cursor_;
  size_t curFrame_;
  size_t lostCount_;
  uint32_t recovered_;
  uint32_t lostFrames_;
  Slot slots_[kMaxPackets];
};

}  // namespace lcr
