// Interfacce verso l'hardware: la logica del protocollo non dipende dalla
// scheda e viene testata sul PC con implementazioni finte (tests/fakes.h).
#pragma once

#include <cstddef>
#include <cstdint>

#include "link_test.h"
#include "position.h"

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
  // SNR (dB) dell'ultimo pacchetto ricevuto, se la radio lo fornisce.
  virtual bool hasSnr() const { return false; }
  virtual float lastSnr() const { return 0.0f; }
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

enum class DenyReason { Receiving, ChannelBusy, Transmitting, NoCallsign };

// Notifiche per display, LED e segnali acustici. Tutte facoltative.
class INotifier {
 public:
  virtual ~INotifier() {}
  virtual void txStarted() {}
  virtual void txEnded(bool turnLimitReached) { (void)turnLimitReached; }
  virtual void pttDenied(DenyReason r) { (void)r; }
  virtual void rxStarted() {}
  virtual void rxCallsign(const char* callsign) { (void)callsign; }
  // Chi trasmette ha mandato la sua posizione (nel pacchetto START). Una volta per ricezione.
  virtual void rxPosition(const Position& p) { (void)p; }
  // Avanzamento (1-100) dell'invio di un messaggio o della ricezione di un messaggio, quando la lunghezza e' dichiarata.
  virtual void txProgress(int percent) { (void)percent; }
  virtual void rxProgress(int percent) { (void)percent; }
  // Inizia la riproduzione di una ricezione (nel profilo Messaggio avviene a ricezione conclusa).
  virtual void rxPlaybackStarted() {}
  virtual void rxEnded(const char* callsign, uint32_t lostFrames, uint32_t recoveredPackets) {
    (void)callsign;
    (void)lostFrames;
    (void)recoveredPackets;
  }
  // Messaggio in casella (un solo messaggio alla volta, vedi inbox.h): vocale (isText = false, dopo rxEnded) o di testo.
  // Non si riproduce da solo: serve un clic breve sul PTT per aprirlo.
  virtual void messageReceived(bool isText, const char* callsign) {
    (void)isText;
    (void)callsign;
  }
  // Il clic sul PTT ha aperto il messaggio: la voce parte (finisce con messageClosed), il testo resta in mostra.
  virtual void messageOpened(bool isText) { (void)isText; }
  // Messaggio ascoltato, letto o interrotto: cancellato dalla casella.
  virtual void messageClosed() {}
  // Modo "registra e poi trasmetti": fase in corso (0 = nessuna, 1 = registra, 2 = attesa, 3 = trasmette).
  virtual void recPhaseChanged(int phase) { (void)phase; }
  // La prova di collegamento e' stata attivata o disattivata da tre clic sul PTT.
  virtual void testModeChanged(bool on) { (void)on; }
  // Esito di una prova di collegamento. fromRemote = false: misurato qui sul pacchetti ricevuti;
  // true: risposta della stazione 'callsign' alla prova trasmessa da questa scheda.
  virtual void linkTestReport(bool fromRemote, const char* callsign, const TestReport& r) {
    (void)fromRemote;
    (void)callsign;
    (void)r;
  }
};

}  // namespace lcr
