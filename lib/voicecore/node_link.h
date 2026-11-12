// Collegamento fra due nodi VoLoRa via UDP ("VoLoRa-IP", docs/studio-nodo-internet.md): un datagramma firmato con HMAC-SHA256, anti-ripetizione
// con contatore e sfida dopo un riavvio. Qui c'e' solo la logica, senza rete ne' orologio: provata sul PC (tests/test_node_link.cpp).
//
// Datagramma (numeri big-endian):
//   0   'V' 'L'                    riconoscimento
//   2   versione (1)               kNodeVersion
//   3   tipo (1)                   NodeMsg
//   4   canale (1)                 0 al primo passo (servira' al reflector)
//   5   id del nodo (4)
//   9   id di avvio (4)            casuale a ogni accensione
//   13  contatore (4)              cresce a ogni datagramma del nodo
//   17  lunghezza del contenuto (1)
//   18  contenuto
//   ... firma (8): HMAC-SHA256 troncato, calcolato su tutto quello che precede, con la chiave condivisa di 16 byte
//
// Tipi: HELLO e KEEPALIVE (contenuto = 5 byte: bit 0 del primo = "ho autenticato il tuo id di avvio", bit 1 = "il collegamento per me e' attivo", poi quell'id di avvio), CHALLENGE (sfida: contenuto =
// 8 byte casuali), RESPONSE (risposta: contenuto = gli stessi 8 byte), PACKET (contenuto = un pacchetto radio cosi' com'e').
// Il collegamento e' "attivo" per spedire solo quando i due nodi si sono autenticati a vicenda: ognuno dice all'altro, nei suoi HELLO e KEEPALIVE, che ha gia'
// accettato il suo id di avvio, cosi' i pacchetti non partono prima che l'altro li accetti.
//
// Regole: un datagramma con firma sbagliata si scarta. Un nodo e' "autenticato" quando ha risposto a una nostra sfida con il suo id di avvio
// attuale: da quel momento si accettano i suoi datagrammi con contatore crescente. Un datagramma con un id di avvio che non conosciamo (il nodo
// si e' riavviato, o e' una vecchia registrazione) NON e' accettato: si risponde con una sfida (al massimo una al secondo) e chi non ha la
// chiave non puo' rispondere; una registrazione riascoltata con l'id di avvio giusto ha un contatore troppo basso. Il contenuto non e' cifrato.
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

constexpr uint8_t kNodeVersion = 1;
constexpr size_t kNodeHeaderBytes = 18;
constexpr size_t kNodeSigBytes = 8;
constexpr size_t kNodeMaxPayload = 255;
constexpr size_t kNodeMaxDatagram = kNodeHeaderBytes + kNodeMaxPayload + kNodeSigBytes;
constexpr size_t kNodeNonceBytes = 8;
constexpr size_t kNodeKeyBytes = 16;

constexpr uint32_t kNodeHelloMs = 2000;           // finche' il collegamento non e' attivo
constexpr uint32_t kNodeKeepaliveMs = 20000;      // collegamento attivo
constexpr uint32_t kNodeDeadMs = 60000;           // senza datagrammi validi il collegamento e' "giu'" (si riparte con HELLO)
constexpr uint32_t kNodeChallengeGapMs = 1000;    // una sfida al secondo al massimo
constexpr uint32_t kNodeChallengeLifeMs = 10000;  // la risposta alla nostra sfida deve arrivare entro questo tempo

enum NodeMsg : uint8_t { kNodeHello = 1, kNodeChallenge = 2, kNodeResponse = 3, kNodePacket = 4, kNodeKeepalive = 5 };

class NodeLink {
 public:
  struct Config {
    uint32_t nodeId = 0;
    uint32_t peerId = 0;  // 0 = qualunque nodo con la chiave giusta
    uint8_t key[kNodeKeyBytes] = {0};
    uint8_t channel = 0;
  };

  enum class Kind : uint8_t {
    Ignored,       // non e' un datagramma VoLoRa-IP, o e' per un altro nodo/canale
    BadSignature,  // firma sbagliata
    Replay,        // contatore troppo basso
    Unknown,       // id di avvio sconosciuto: risposta con una sfida (se non e' troppo presto)
    Control,       // HELLO, CHALLENGE, RESPONSE, KEEPALIVE valido
    Packet,        // pacchetto radio valido da ritrasmettere (packet, packetLen)
  };

  struct Reply {
    uint8_t data[kNodeMaxDatagram];
    size_t len = 0;
  };

  struct Rx {
    Kind kind = Kind::Ignored;
    const uint8_t* packet = nullptr;  // dentro il datagramma ricevuto: valido finche' il buffer resta
    size_t packetLen = 0;
    Reply replies[2];  // datagrammi da rispedire al nodo (sfida, risposta)
    size_t nReplies = 0;
  };

  struct Stats {
    uint32_t sent = 0, received = 0, badSignature = 0, replays = 0, unknown = 0, ignored = 0, challenges = 0, authenticated = 0;
  };

  // bootId e rng: l'id di avvio casuale e il generatore dei numeri della sfida (sul PC si danno valori fissi).
  NodeLink(const Config& cfg, uint32_t bootId, uint32_t (*rng)());

  // Datagrammi da mandare al nodo.
  size_t makeHello(uint8_t* out, size_t cap);
  size_t makeKeepalive(uint8_t* out, size_t cap);
  size_t makePacket(const uint8_t* wire, size_t len, uint8_t* out, size_t cap);  // 0 se len e' troppo lungo

  Rx& onDatagram(const uint8_t* d, size_t n, uint32_t nowMs);

  // Il collegamento e' attivo: il nodo e' autenticato e si sono visti datagrammi validi negli ultimi kNodeDeadMs.
  bool linkUp(uint32_t nowMs) const { return peerAuth_ && peerAcceptsMe_ && haveRx_ && nowMs - lastRxMs_ < kNodeDeadMs; }
  bool peerAuthenticated() const { return peerAuth_; }
  uint32_t peerId() const { return peerSeenId_; }
  // Cosa mandare ora per tenere il collegamento: HELLO ogni kNodeHelloMs se non e' attivo, KEEPALIVE ogni kNodeKeepaliveMs se lo e'.
  // Ritorna la lunghezza (0 = niente da mandare); aggiorna il proprio orologio.
  size_t maintenance(uint32_t nowMs, uint8_t* out, size_t cap);

  const Stats& stats() const { return stats_; }

 private:
  size_t build(uint8_t type, const uint8_t* payload, size_t len, uint8_t* out, size_t cap);
  void makeChallenge(uint32_t nowMs, Reply& r);

  Config cfg_;
  uint32_t boot_;
  uint32_t (*rng_)();
  uint32_t counter_ = 0;
  // nodo dall'altra parte
  bool peerAuth_ = false;        // abbiamo autenticato l'id di avvio dell'altro nodo
  bool haveStatusReplyMs_ = false;
  uint32_t lastStatusReplyMs_ = 0;
  bool peerAcceptsMe_ = false;   // l'altro nodo ci ha detto di aver autenticato il nostro id di avvio
  size_t makeStatus(uint8_t type, uint8_t* out, size_t cap);
  uint32_t peerSeenId_ = 0;
  uint32_t peerBoot_ = 0;
  uint32_t peerLastCounter_ = 0;
  bool haveRx_ = false;
  uint32_t lastRxMs_ = 0;
  // sfida emessa
  bool challengePending_ = false;
  uint8_t nonce_[kNodeNonceBytes] = {0};
  uint32_t challengeMs_ = 0;
  bool haveChallengeMs_ = false;
  uint32_t lastChallengeMs_ = 0;
  // manutenzione
  bool haveSent_ = false;
  uint32_t lastSendMs_ = 0;
  Stats stats_;
  Rx rx_;
};

}  // namespace lcr
