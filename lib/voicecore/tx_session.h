// Lato trasmissione: trasforma i frame Codec2 in pacchetti.
//
// Sequenza prodotta (con parita' attiva):
//   seq 0       START + nominativo, nessun frame
//   seq 1..3    dati (12 frame ciascuno)
//   seq 4       parita' dei seq 0..3
//   seq 5..8    dati, 9 parita' ...
//   ultimo dato END + nominativo (anche con 0 frame), poi parita' del gruppo parziale
#pragma once

#include <cstddef>
#include <cstdint>

#include "packet.h"
#include "protocol.h"

namespace lcr {

class TxSession {
 public:
  TxSession() = default;
  TxSession(const char* callsign, uint16_t session, bool fec, uint8_t mode = kModeCodec2_1200);

  // Accoda il pacchetto iniziale col nominativo. Chiamato in automatico da pushFrame.
  void begin();
  // Aggiunge un frame Codec2 (kFrameBytes). Ritorna false se il limite di
  // pacchetti e' raggiunto o la coda e' piena: il frame e' scartato.
  bool pushFrame(const uint8_t* frame);
  // Chiude la trasmissione: pacchetto END con i frame residui e parita' finale.
  void finish();

  bool hasPacket() const { return qCount_ > 0; }
  bool popPacket(Packet& out);
  bool finished() const { return finished_; }
  bool overflowed() const { return overflow_; }
  uint8_t nextSeq() const { return seq_; }
  uint16_t session() const { return session_; }

 private:
  bool emitData(uint8_t extraFlags);
  void emitParity();
  bool enqueue(const Packet& p);

  static constexpr size_t kQueue = 8;

  char callsign_[kMaxCallsign + 1] = {0};
  uint16_t session_ = 0;
  bool fec_ = true;
  uint8_t mode_ = kModeCodec2_1200;
  bool begun_ = false;
  bool finished_ = false;
  bool overflow_ = false;

  uint8_t seq_ = 0;
  uint8_t lastCallsignSeq_ = 0;

  uint8_t frames_[kMaxDataPayload] = {0};
  size_t frameCount_ = 0;
  ParityAccumulator parity_;

  Packet q_[kQueue];
  size_t qHead_ = 0;
  size_t qCount_ = 0;
};

}  // namespace lcr
