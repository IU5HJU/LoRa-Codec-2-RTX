// Collegamento seriale fra le due schede del ripetitore e coda di trasmissione (docs/studio-ripetitore.md).
//
// La scheda RX manda alla scheda TX i pacchetti da ritrasmettere (e un battito ogni secondo); la TX li mette in coda e li
// trasmette uno dopo l'altro. Qui c'e' solo la parte senza hardware, provata sul PC (tests/test_repeater_link.cpp).
//
// Formato di un frame sulla seriale (115200 8N1):
//   0x7E | tipo | lunghezza N | N byte | CRC16 (alto, basso)
// Il CRC16 (CCITT, polinomio 0x1021, partenza 0xFFFF) copre tipo, lunghezza e dati. Il lettore si risincronizza da solo:
// con un CRC errato scarta il primo byte e cerca il prossimo 0x7E.
#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.h"

namespace lcr {

constexpr uint8_t kLinkStart = 0x7E;
enum LinkType : uint8_t {
  kLinkPacket = 1,     // dati: un pacchetto radio completo (come esce da encodePacket)
  kLinkHeartbeat = 2,  // dati: 4 byte, numero di pacchetti ricevuti dalla RX (big endian); la TX sa che il collegamento vive
  kLinkStatus = 3,     // dati: 2 byte, pacchetti in coda e pacchetti persi per coda piena (dalla TX alla RX)
};
constexpr size_t kLinkMaxData = kMaxWireBytes;                // un pacchetto radio
constexpr size_t kLinkMaxFrame = 3 + kLinkMaxData + 2;

uint16_t linkCrc16(const uint8_t* data, size_t len);

// Scrive un frame in out. Ritorna i byte scritti, 0 se i dati sono troppi o out e' troppo piccolo.
size_t encodeLinkFrame(uint8_t type, const uint8_t* data, size_t len, uint8_t* out, size_t cap);

class LinkParser {
 public:
  struct Frame {
    uint8_t type;
    uint8_t len;
    uint8_t data[kLinkMaxData];
  };

  // Aggiunge un byte. Ritorna true se e' completo un frame valido: lo si legge con frame(); il frame resta valido fino al
  // byte successivo.
  bool feed(uint8_t b);
  const Frame& frame() const { return frame_; }
  uint32_t crcErrors() const { return crcErrors_; }
  void reset() { n_ = 0; }

 private:
  void drop(size_t k);
  uint8_t buf_[kLinkMaxFrame];
  size_t n_ = 0;
  Frame frame_;
  uint32_t crcErrors_ = 0;
};

// Coda dei pacchetti da trasmettere, con la pausa minima fra una trasmissione e la successiva (kInterPacketGapMs).
class TxQueue {
 public:
  static constexpr size_t kSlots = 16;

  // false se la coda e' piena: il pacchetto e' perso e contato.
  bool push(const uint8_t* data, size_t len);
  // Se c'e' un pacchetto pronto (coda non vuota e pausa trascorsa) lo copia in out, ne scrive la lunghezza in len e ritorna
  // true. Dopo la trasmissione chiamare sent().
  bool next(uint32_t nowMs, uint8_t* out, size_t cap, size_t& len);
  void sent(uint32_t nowMs) { lastSentMs_ = nowMs; haveSent_ = true; }
  size_t size() const { return count_; }
  uint32_t dropped() const { return dropped_; }

 private:
  uint8_t data_[kSlots][kLinkMaxData];
  size_t len_[kSlots];
  size_t head_ = 0;
  size_t count_ = 0;
  uint32_t lastSentMs_ = 0;
  bool haveSent_ = false;
  uint32_t dropped_ = 0;
};

}  // namespace lcr
