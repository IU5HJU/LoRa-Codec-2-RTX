// Rete UDP del nodo e del ripetitore (docs/studio-reflector.md): un task sul core 0 e un solo socket, quello della porta locale, da cui si
// riceve E si spedisce. Spedire dallo stesso socket e' essenziale con un reflector: risponde all'indirizzo e alla porta da cui e' arrivato il
// datagramma (quelli messi dal NAT del router), quindi la risposta deve poter tornare proprio a questo socket, senza aprire porte sul router.
// Il task risolve anche il nome del reflector (lib/voicecore/peer_resolve.h) e lo riprova a ogni cambio di indirizzo.
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "node_core.h"
#include "settings.h"

class NetUdp : public lcr::INodeNet {
 public:
  // Avvia il task. host: indirizzo IPv4 in cifre o nome; peerPort: porta dell'altro nodo o del reflector; localPort: porta locale di ascolto.
  void begin(uint16_t localPort, const char* host, uint16_t peerPort);
  // In coda: lo spedisce il task (dallo stesso socket di ricezione).
  void sendDatagram(const uint8_t* data, size_t len) override;
  // Un datagramma ricevuto, se ce n'e' (len = byte copiati, al massimo cap).
  bool receive(uint8_t* out, size_t cap, size_t& len);
  // Il chiamante dice se il collegamento e' attivo: se non lo e' il nome si risolve piu' spesso.
  void setLinkUp(bool up) { linkUp_ = up; }
  // L'indirizzo dell'altro nodo (0 = non ancora risolto) e quante volte e' cambiato.
  uint32_t peerAddress() const { return peerAddr_; }
  uint32_t resolveCount() const { return resolved_; }
  // L'indirizzo IPv4 pubblico della scheda come lo vede Internet (a<<24|b<<16|c<<8|d; 0 = non ancora noto o funzione spenta). Lo chiede il task a un
  // servizio esterno (LCR_PUBLIC_IP_HOST in user_config.h) al primo collegamento Wi-Fi e poi ogni 10 minuti; serve al reflector, che dietro un router
  // vedrebbe solo l'indirizzo del router.
  uint32_t publicAddress() const { return publicAddr_; }

 private:
  struct Dgram {
    uint16_t len;
    uint8_t data[lcr::kNodeMaxDatagram];
  };
  static void taskEntry(void* self) { static_cast<NetUdp*>(self)->run(); }
  void run();
  void fetchPublicAddress();
  static void fetchEntry(void* self);

  QueueHandle_t rx_ = nullptr;
  QueueHandle_t tx_ = nullptr;
  uint16_t localPort_ = 0;
  uint16_t peerPort_ = 0;
  char host_[lcr::kMaxPeerHost + 1] = {0};
  volatile uint32_t peerAddr_ = 0;
  volatile uint32_t resolved_ = 0;
  volatile uint32_t publicAddr_ = 0;
  volatile bool fetching_ = false;  // la richiesta dell'indirizzo pubblico e' in corso (gira in un task a parte: puo' durare qualche secondo)
  volatile bool linkUp_ = false;
};
