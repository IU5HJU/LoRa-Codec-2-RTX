// Logica del ripetitore a due schede (studio: docs/studio-ripetitore.md, opzione A).
//
// La scheda RX riceve un pacchetto con CRC valido e lo decodifica; Repeater decide se ritrasmetterlo e come. La scheda TX
// ritrasmette il pacchetto ottenuto sulla frequenza di uscita. Qui non c'e' radio, seriale ne' orologio: solo la decisione,
// cosi' si prova sul PC.
//
// Regole:
//   - il pacchetto ritrasmesso e' identico all'originale (stessa sessione, sequenza, nominativo, posizione, payload) con in
//     piu' il flag kFlagRelayed;
//   - un pacchetto che ha gia' kFlagRelayed non si ripete (niente anelli);
//   - lo stesso pacchetto (sessione e sequenza) si ripete una sola volta (memoria degli ultimi pacchetti);
//   - si ripetono solo i modi scelti (di base il solo Codec2 1200: il ripetitore non guarda il contenuto, ma il modo si);
//   - una stessa sessione non si ripete oltre un tempo massimo (un turno dura al massimo 30 s): una stazione bloccata in
//     trasmissione non tiene occupato il ripetitore.
#pragma once

#include <cstddef>
#include <cstdint>

#include "packet.h"
#include "protocol.h"

namespace lcr {

class Repeater {
 public:
  enum class Verdict : uint8_t {
    Forward,         // ritrasmettere (out e' pronto)
    Disabled,        // ripetitore spento
    AlreadyRelayed,  // il pacchetto e' gia' stato ripetuto da un ripetitore
    ModeNotRepeated, // modo non compreso fra quelli ripetuti
    Duplicate,       // sessione e sequenza gia' ripetute da poco
    SessionTooLong,  // la sessione dura oltre il massimo
  };

  struct Stats {
    uint32_t forwarded = 0;
    uint32_t disabled = 0;
    uint32_t alreadyRelayed = 0;
    uint32_t modeNotRepeated = 0;
    uint32_t duplicate = 0;
    uint32_t sessionTooLong = 0;
  };

  static constexpr size_t kDedupSlots = 64;
  static constexpr uint32_t kDedupWindowMs = 10000;       // un pacchetto uguale entro questo tempo e' un duplicato
  static constexpr uint32_t kDefaultMaxSessionMs = 35000; // 30 s di turno + margine

  Repeater() { reset(); }
  void reset();

  void setEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }
  // Maschera dei modi ripetuti: il bit n (1 << modo). Di base solo il Codec2 1200.
  void setModeMask(uint16_t mask) { modeMask_ = mask; }
  uint16_t modeMask() const { return modeMask_; }
  void setMaxSessionMs(uint32_t ms) { maxSessionMs_ = ms; }

  // 'in' e' un pacchetto decodificato dalla scheda RX. Con Verdict::Forward, 'out' e' il pacchetto da trasmettere.
  Verdict onPacket(const Packet& in, uint32_t nowMs, Packet& out);

  const Stats& stats() const { return stats_; }

 private:
  struct Slot {
    bool used;
    uint16_t session;
    uint8_t seq;
    uint32_t atMs;
  };
  bool seenRecently(uint16_t session, uint8_t seq, uint32_t nowMs) const;
  void remember(uint16_t session, uint8_t seq, uint32_t nowMs);

  bool enabled_ = true;
  uint16_t modeMask_ = 1u << kModeCodec2_1200;
  uint32_t maxSessionMs_ = kDefaultMaxSessionMs;
  Slot slots_[kDedupSlots];
  size_t next_ = 0;
  bool haveSession_ = false;
  uint16_t curSession_ = 0;
  uint32_t curStartMs_ = 0;
  Stats stats_;
};

}  // namespace lcr
