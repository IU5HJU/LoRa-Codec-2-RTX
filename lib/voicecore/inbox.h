// Casella dei messaggi ricevuti: UN solo messaggio alla volta (vocale oppure di testo). Un messaggio nuovo cancella il
// vecchio; dopo l'ascolto (o la lettura) il messaggio viene cancellato. Tutto in RAM: dopo un riavvio la casella e' vuota.
// Memoria: voce 30 s Codec2 1200 = 750 frame da 6 byte (4500 byte) piu' la mappa dei frame persi; testo fino a 150 caratteri.
// Indipendente dalla scheda e testato sul PC (tests/test_inbox.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.h"

namespace lcr {

class Inbox {
 public:
  enum class Kind : uint8_t { None, Voice, Text };

  void clear();
  Kind kind() const { return kind_; }
  bool has() const { return kind_ != Kind::None; }
  const char* callsign() const { return callsign_; }

  // Voce: beginVoice() svuota la casella, poi pushFrame()/pushLost() in ordine, infine commitVoice() (false e casella vuota
  // se non e' arrivato nessun frame).
  void beginVoice();
  bool pushFrame(const uint8_t* frame);  // kFrameBytes byte; false se la casella e' piena
  bool pushLost();
  bool commitVoice(const char* callsign);
  size_t frames() const { return frames_; }
  // Copia il frame i in 'out' (kFrameBytes byte). false se il frame e' perso (o i fuori intervallo).
  bool frameAt(size_t i, uint8_t* out) const;

  // Testo: sostituisce il contenuto. false (casella invariata) se il testo non e' valido.
  bool setText(const char* callsign, const char* text, size_t len);
  const char* text() const { return text_; }
  size_t textLen() const { return textLen_; }

 private:
  void setCallsign(const char* cs);

  Kind kind_ = Kind::None;
  char callsign_[kMaxCallsign + 1] = {0};
  uint8_t frameBuf_[kMaxTurnFrames * kFrameBytes];
  uint8_t lost_[(kMaxTurnFrames + 7) / 8];
  size_t frames_ = 0;
  char text_[kMaxTextChars + 1] = {0};
  size_t textLen_ = 0;
};

}  // namespace lcr
