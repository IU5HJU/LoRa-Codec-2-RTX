// Interfacce verso l'hardware: la logica del protocollo non dipende dalla
// scheda e viene testata sul PC con implementazioni finte (tests/fakes.h).
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

class IRadio {
 public:
  virtual ~IRadio() {}
  // true se il canale e' occupato (CAD / rilevamento preambolo).
  virtual bool channelBusy() = 0;
  // Trasmette un pacchetto (bloccante). Dopo la trasmissione la radio
  // resta in standby finche' non si chiama startReceive().
  virtual bool send(const uint8_t* data, size_t len) = 0;
  virtual void startReceive() = 0;
  // Non bloccante. Ritorna i byte copiati in buf, 0 se non c'e' nulla.
  virtual size_t receive(uint8_t* buf, size_t cap, int* rssi) = 0;
};

class ICodec {
 public:
  virtual ~ICodec() {}
  // 320 campioni PCM 16 bit a 8 kHz -> 6 byte (Codec2 1200).
  virtual void encode(const int16_t* pcm, uint8_t* frame) = 0;
  virtual void decode(const uint8_t* frame, int16_t* pcm) = 0;
};

class IAudioIn {
 public:
  virtual ~IAudioIn() {}
  virtual void start() = 0;  // inizia a catturare dal microfono
  virtual void stop() = 0;
  // Non bloccante. true e riempie 320 campioni se un frame da 40 ms e' pronto.
  virtual bool readFrame(int16_t* pcm) = 0;
};

class IAudioOut {
 public:
  virtual ~IAudioOut() {}
  virtual void playFrame(const int16_t* pcm) = 0;  // 320 campioni, 40 ms
  virtual void playLost() = 0;                     // 40 ms di frame perso
};

enum class DenyReason { Receiving, ChannelBusy, Transmitting };

// Notifiche per display, LED e segnali acustici. Tutte facoltative.
class INotifier {
 public:
  virtual ~INotifier() {}
  virtual void txStarted() {}
  virtual void txEnded(bool turnLimitReached) { (void)turnLimitReached; }
  virtual void pttDenied(DenyReason r) { (void)r; }
  virtual void rxStarted() {}
  virtual void rxCallsign(const char* callsign) { (void)callsign; }
  virtual void rxEnded(const char* callsign, uint32_t lostFrames, uint32_t recoveredPackets) {
    (void)callsign;
    (void)lostFrames;
    (void)recoveredPackets;
  }
};

}  // namespace lcr
