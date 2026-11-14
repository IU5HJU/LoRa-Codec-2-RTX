// Il reflector VoLoRa (docs/studio-reflector.md): un server UDP a cui si collegano nodi e ripetitori. Ogni client e' un collegamento VoLoRa-IP
// (node_link.h) con la sua chiave; il reflector inoltra i pacchetti radio validi da un client a tutti gli altri collegati.
//   - I client scrivono per primi e il reflector risponde all'indirizzo e alla porta da cui e' arrivato il datagramma: nessuna porta aperta sul
//     router dei client (come nel reflector YSF). L'indirizzo di un client si aggiorna a ogni datagramma firmato valido (NAT che cambia porta).
//   - Un datagramma da un id sconosciuto si ignora senza rispondere (nessuna amplificazione); uno con firma sbagliata si scarta.
//   - Voce (solo il modo del reflector): chi parla per primo tiene il canale fino al pacchetto di fine o a kTalkHangMs senza pacchetti; le voci
//     degli altri nel frattempo si scartano (contate). Il testo (modo 7) passa sempre. Gli altri modi si scartano.
//   - Ogni pacchetto inoltrato ha il flag RELAYED.
// Solo logica, senza socket ne' orologio: provata sul PC (tests/test_reflector_core.cpp); tools/vlreflector.cpp ci mette i socket.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "node_link.h"

namespace lcr {

// true se l'indirizzo IPv4 (a<<24|b<<16|c<<8|d) e' un indirizzo pubblico, cioe' non e' di una rete locale (10/8, 172.16/12, 192.168/16), ne' di loopback,
// link-local, CGNAT degli operatori (100.64/10), "questa rete" (0/8), multicast o riservato (224 e oltre).
inline bool isPublicIpv4(uint32_t a) {
  const uint32_t o1 = a >> 24, o2 = (a >> 16) & 255;
  if (o1 == 0 || o1 == 10 || o1 == 127 || o1 >= 224) return false;
  if (o1 == 172 && o2 >= 16 && o2 <= 31) return false;
  if (o1 == 192 && o2 == 168) return false;
  if (o1 == 169 && o2 == 254) return false;
  if (o1 == 100 && o2 >= 64 && o2 <= 127) return false;
  return true;
}

typedef uint64_t Endpoint;  // indirizzo e porta di un client, come li tiene chi ha il socket (per IPv4: indirizzo * 65536 + porta)

class IReflectorNet {
 public:
  virtual ~IReflectorNet() {}
  virtual void send(Endpoint to, const uint8_t* data, size_t len) = 0;
};

class ReflectorCore {
 public:
  static constexpr uint32_t kTalkHangMs = 3000;  // senza pacchetti di voce per tanto, il canale si libera

  struct Stats {
    uint32_t datagrams = 0;       // datagrammi ricevuti
    uint32_t unknownClient = 0;   // id non in elenco (ignorati, senza risposta)
    uint32_t badSignature = 0;
    uint32_t replays = 0;
    uint32_t voiceForwarded = 0;  // pacchetti di voce inoltrati (uno per client di destinazione)
    uint32_t textForwarded = 0;
    uint32_t voiceBlocked = 0;    // voce di un altro client mentre uno parlava
    uint32_t badMode = 0;         // modo non inoltrato
    uint32_t badPacket = 0;       // non e' un pacchetto radio valido
    uint32_t talks = 0;           // trasmissioni iniziate
  };

  // Una trasmissione (finita o in corso): chi l'ha fatta arrivare e con quale nominativo.
  struct Talk {
    uint32_t clientId = 0;
    std::string client;      // nome del client (il suo nominativo)
    std::string callsign;    // nominativo di chi trasmetteva (dal pacchetto di inizio), "-" se manca
    uint32_t startMs = 0;
    uint32_t lastMs = 0;     // ultimo pacchetto
    uint32_t packets = 0;
    bool active = false;
  };
  static constexpr size_t kRecentTalks = 20;

  struct ClientInfo {
    uint32_t id = 0;
    std::string name;
    bool haveEndpoint = false;
    Endpoint endpoint = 0;
    bool up = false;        // collegamento attivo (autenticati a vicenda)
    uint32_t lastSeenMs = 0;
    uint32_t packetsIn = 0;
    uint32_t packetsOut = 0;
    uint32_t declaredAddr = 0;  // l'indirizzo pubblico che il nodo dice di avere (a<<24|b<<16|c<<8|d), 0 = non lo dice; non verificato
  };

  // reflectorId: l'id con cui il reflector si presenta (il PEERID dei client); voiceMode: il modo voce inoltrato (di base 4, Standard).
  ReflectorCore(IReflectorNet& net, uint32_t reflectorId, uint8_t voiceMode, uint32_t bootId, uint32_t (*rng)());

  // Un client ammesso: id (diverso da 0 e da quello del reflector), chiave di 16 byte, un nome per il registro. false se non e' valido o c'e' gia'.
  bool addClient(uint32_t id, const uint8_t key[kNodeKeyBytes], const char* name);
  // Gestione a reflector acceso (registrazione dei nodi, ricaricamento del file): cambiare la chiave stacca il client (deve ripresentarsi con la
  // nuova); togliere un client lo ferma subito. false se l'id non c'e'.
  bool setClientKey(uint32_t id, const uint8_t key[kNodeKeyBytes]);
  bool setClientName(uint32_t id, const char* name);
  bool removeClient(uint32_t id);
  bool hasClient(uint32_t id) const;
  uint32_t idByName(const char* name) const;  // 0 se non c'e' (il nome si confronta senza distinguere maiuscole)

  void onDatagram(Endpoint from, const uint8_t* d, size_t n, uint32_t nowMs);
  // Manutenzione: HELLO e KEEPALIVE ai client collegati, e dimenticare l'indirizzo di chi tace da kNodeDeadMs.
  void tick(uint32_t nowMs);

  std::vector<ClientInfo> clients(uint32_t nowMs) const;
  const Stats& stats() const { return stats_; }
  // Chi sta parlando adesso (0 = nessuno).
  uint32_t talker(uint32_t nowMs) const { return talking_ && nowMs - talkLastMs_ <= kTalkHangMs ? talkerId_ : 0; }
  // La trasmissione in corso (active) e le ultime finite, la piu' recente per prima, al massimo kRecentTalks.
  Talk currentTalk(uint32_t nowMs) const;
  const std::vector<Talk>& recentTalks() const { return recent_; }
  // Registro (facoltativo): una riga per evento (client collegato, trasmissione iniziata e finita, ...).
  void setLog(std::function<void(const std::string&)> f) { log_ = f; }

 private:
  struct Client {
    uint32_t id;
    std::string name;
    std::unique_ptr<NodeLink> link;
    bool haveEndpoint = false;
    Endpoint endpoint = 0;
    uint32_t lastSeenMs = 0;
    bool wasUp = false;
    uint32_t packetsIn = 0, packetsOut = 0;
  };
  Client* find(uint32_t id);
  const Client* find(uint32_t id) const;
  void endTalk();
  void forward(const Client& from, const uint8_t* wire, size_t len, bool voice, uint32_t nowMs);
  void say(const std::string& s) const;

  IReflectorNet& net_;
  uint32_t id_;
  uint8_t voiceMode_;
  uint32_t boot_;
  uint32_t (*rng_)();
  std::vector<std::unique_ptr<Client>> clients_;
  Stats stats_;
  bool talking_ = false;
  uint32_t talkerId_ = 0;
  uint16_t talkSession_ = 0;
  uint32_t talkLastMs_ = 0;
  Talk cur_;
  std::vector<Talk> recent_;
  std::function<void(const std::string&)> log_;
};

}  // namespace lcr
