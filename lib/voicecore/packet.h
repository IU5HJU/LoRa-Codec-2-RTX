// Formato dei pacchetti e parita' XOR.
#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.h"

namespace lcr {

struct Packet {
  uint8_t mode = kModeCodec2_1200;
  uint16_t session = 0;  // ID casuale della trasmissione (PTT)
  uint8_t seq = 0;
  uint8_t flags = 0;
  char callsign[kMaxCallsign + 1] = {0};  // vuoto se assente
  uint8_t payloadLen = 0;
  uint8_t payload[kMaxPayload] = {0};

  bool isParity() const { return (flags & kFlagParity) != 0; }
  bool hasCallsign() const { return callsign[0] != 0; }
};

// Nominativo: 1-8 caratteri tra A-Z, 0-9 e '/'.
bool isValidCallsign(const char* cs);

// Serializza. Ritorna i byte scritti, 0 se non entra o il nominativo non e' valido.
// Il flag kFlagCallsign e' impostato automaticamente.
size_t encodePacket(const Packet& p, uint8_t* out, size_t cap);

// Deserializza e valida. Ritorna false per pacchetti non conformi.
bool decodePacket(const uint8_t* in, size_t len, Packet& out);

// Accumula i pacchetti dati di un gruppo e costruisce il pacchetto di parita'.
class ParityAccumulator {
 public:
  ParityAccumulator() { reset(); }
  void reset();
  void add(const Packet& data);
  size_t count() const { return count_; }
  void build(Packet& out, uint16_t session, uint8_t mode, uint8_t seq) const;

 private:
  uint8_t count_;
  uint8_t xorLen_;
  uint8_t xorFlags_;
  uint8_t maxLen_;
  uint8_t buf_[kMaxDataPayload];
};

// Ricostruisce l'unico pacchetto mancante di un gruppo.
// group[i] e' il pacchetto dati i-esimo del gruppo o nullptr se mancante;
// n deve essere uguale al count scritto nella parita'.
bool recoverFromParity(const Packet& parity, const Packet* const* group, size_t n, Packet& out);

}  // namespace lcr
