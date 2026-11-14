#include "reflector_core.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "packet.h"
#include "protocol.h"

namespace lcr {

namespace {
uint32_t get32(const uint8_t* d) { return (static_cast<uint32_t>(d[0]) << 24) | (static_cast<uint32_t>(d[1]) << 16) | (static_cast<uint32_t>(d[2]) << 8) | d[3]; }
}  // namespace

ReflectorCore::ReflectorCore(IReflectorNet& net, uint32_t reflectorId, uint8_t voiceMode, uint32_t bootId, uint32_t (*rng)())
    : net_(net), id_(reflectorId), voiceMode_(voiceMode), boot_(bootId), rng_(rng) {}

void ReflectorCore::say(const std::string& s) const {
  if (log_) log_(s);
}

bool ReflectorCore::addClient(uint32_t id, const uint8_t key[kNodeKeyBytes], const char* name) {
  if (id == 0 || id == id_ || find(id)) return false;
  std::unique_ptr<Client> c(new Client);
  c->id = id;
  c->name = name ? name : "";
  NodeLink::Config cfg;
  cfg.nodeId = id_;
  cfg.peerId = id;
  memcpy(cfg.key, key, kNodeKeyBytes);
  c->link.reset(new NodeLink(cfg, boot_ + static_cast<uint32_t>(clients_.size()) + 1, rng_));
  clients_.push_back(std::move(c));
  return true;
}

ReflectorCore::Client* ReflectorCore::find(uint32_t id) {
  for (auto& c : clients_) {
    if (c->id == id) return c.get();
  }
  return nullptr;
}

const ReflectorCore::Client* ReflectorCore::find(uint32_t id) const {
  for (const auto& c : clients_) {
    if (c->id == id) return c.get();
  }
  return nullptr;
}

bool ReflectorCore::hasClient(uint32_t id) const { return find(id) != nullptr; }

static bool sameName(const std::string& a, const char* b) {
  const size_t n = strlen(b);
  if (a.size() != n) return false;
  for (size_t i = 0; i < n; i++) {
    if (toupper(static_cast<unsigned char>(a[i])) != toupper(static_cast<unsigned char>(b[i]))) return false;
  }
  return true;
}

uint32_t ReflectorCore::idByName(const char* name) const {
  if (!name || !name[0]) return 0;
  for (const auto& c : clients_) {
    if (sameName(c->name, name)) return c->id;
  }
  return 0;
}

bool ReflectorCore::setClientKey(uint32_t id, const uint8_t key[kNodeKeyBytes]) {
  Client* c = find(id);
  if (!c) return false;
  NodeLink::Config cfg;
  cfg.nodeId = id_;
  cfg.peerId = id;
  memcpy(cfg.key, key, kNodeKeyBytes);
  c->link.reset(new NodeLink(cfg, boot_ + id + 0x1000, rng_));  // un collegamento nuovo: chi ha la chiave vecchia non e' piu' accettato
  c->haveEndpoint = false;
  if (c->wasUp) {
    c->wasUp = false;
    say(std::string("client ") + c->name + " scollegato (chiave cambiata)");
  }
  return true;
}

bool ReflectorCore::setClientName(uint32_t id, const char* name) {
  Client* c = find(id);
  if (!c) return false;
  c->name = name ? name : "";
  return true;
}

bool ReflectorCore::removeClient(uint32_t id) {
  for (size_t i = 0; i < clients_.size(); i++) {
    if (clients_[i]->id != id) continue;
    if (talking_ && talkerId_ == id) {
      talking_ = false;
      endTalk();
    }
    say(std::string("client ") + clients_[i]->name + " tolto");
    clients_.erase(clients_.begin() + static_cast<long>(i));
    return true;
  }
  return false;
}

// Chiude la trasmissione in corso e la mette fra le ultime
void ReflectorCore::endTalk() {
  if (!cur_.active) return;
  cur_.active = false;
  recent_.insert(recent_.begin(), cur_);
  if (recent_.size() > kRecentTalks) recent_.resize(kRecentTalks);
  cur_ = Talk();
}

ReflectorCore::Talk ReflectorCore::currentTalk(uint32_t nowMs) const {
  Talk t = cur_;
  if (t.active && nowMs - talkLastMs_ > kTalkHangMs) t.active = false;  // e' finita per silenzio: la chiude il prossimo giro di tick()
  return t;
}

void ReflectorCore::onDatagram(Endpoint from, const uint8_t* d, size_t n, uint32_t nowMs) {
  stats_.datagrams++;
  // Il client si riconosce dall'id nell'intestazione (e poi dalla firma con la sua chiave): sconosciuto = ignorato, senza risposta
  if (!d || n < kNodeHeaderBytes + kNodeSigBytes || d[0] != 'V' || d[1] != 'L') {
    stats_.unknownClient++;
    return;
  }
  Client* c = find(get32(d + 5));
  if (!c) {
    stats_.unknownClient++;
    return;
  }
  NodeLink::Rx& rx = c->link->onDatagram(d, n, nowMs);
  if (rx.kind == NodeLink::Kind::BadSignature) {
    stats_.badSignature++;
    return;
  }
  if (rx.kind == NodeLink::Kind::Replay) {
    stats_.replays++;
    return;
  }
  // Le risposte (sfida, risposta, stato) vanno a chi ha scritto, anche se non e' ancora autenticato
  for (size_t i = 0; i < rx.nReplies; i++) net_.send(from, rx.replies[i].data, rx.replies[i].len);

  // L'indirizzo del client si aggiorna solo con datagrammi che non si possono riascoltare e rimandare da un altro: PACKET, HELLO, KEEPALIVE
  // (contatore crescente) e RESPONSE (legata alla sfida appena mandata); una SFIDA firmata si potrebbe riascoltare: non sposta niente.
  const uint8_t type = d[3];
  const bool fresh = (rx.kind == NodeLink::Kind::Packet || rx.kind == NodeLink::Kind::Control) && type != kNodeChallenge;
  if (fresh && c->link->peerAuthenticated()) {
    c->haveEndpoint = true;
    c->endpoint = from;
    c->lastSeenMs = nowMs;
  }
  const bool up = c->link->linkUp(nowMs);
  if (up != c->wasUp) {
    c->wasUp = up;
    say(std::string("client ") + c->name + (up ? " collegato" : " non piu' collegato"));
  }

  if (rx.kind != NodeLink::Kind::Packet) return;
  c->packetsIn++;
  Packet p;
  if (!decodePacket(rx.packet, rx.packetLen, p)) {
    stats_.badPacket++;
    return;
  }
  const bool voice = p.mode == voiceMode_;
  if (!voice && p.mode != kModeText) {
    stats_.badMode++;
    return;
  }
  if (voice) {
    const bool hanging = talking_ && nowMs - talkLastMs_ <= kTalkHangMs;
    if (hanging && talkerId_ != c->id) {  // un altro sta parlando: la voce si scarta
      stats_.voiceBlocked++;
      return;
    }
    // La parita' puo' arrivare dopo il pacchetto di fine: non inizia una trasmissione e non tiene il canale (passa solo se e' libero o e' di chi parla)
    if (!p.isParity()) {
      if (!hanging || talkerId_ != c->id || talkSession_ != p.session) {
        endTalk();  // quella di prima (dello stesso client con un'altra sessione) e' finita
        stats_.talks++;
        cur_ = Talk();
        cur_.active = true;
        cur_.clientId = c->id;
        cur_.client = c->name;
        cur_.callsign = p.hasCallsign() ? p.callsign : "-";
        cur_.startMs = nowMs;
        char b[96];
        snprintf(b, sizeof(b), "parla %s (%s), sessione %04X", p.hasCallsign() ? p.callsign : "-", c->name.c_str(), p.session);
        say(b);
      } else if (cur_.active && cur_.callsign == "-" && p.hasCallsign()) {
        cur_.callsign = p.callsign;  // il nominativo puo' arrivare nel pacchetto dopo
      }
      talking_ = true;
      talkerId_ = c->id;
      talkSession_ = p.session;
      talkLastMs_ = nowMs;
      cur_.lastMs = nowMs;
      cur_.packets++;
    } else if (hanging) {
      talkLastMs_ = nowMs;
      cur_.lastMs = nowMs;
    }
  }
  p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(p, wire, sizeof(wire));
  if (m == 0) {
    stats_.badPacket++;
    return;
  }
  forward(*c, wire, m, voice, nowMs);
  if (voice && (p.flags & kFlagEnd) && !p.isParity()) {  // fine della trasmissione: il canale si libera subito
    talking_ = false;
    endTalk();
    say(std::string("fine trasmissione di ") + c->name);
  }
}

void ReflectorCore::forward(const Client& from, const uint8_t* wire, size_t len, bool voice, uint32_t nowMs) {
  for (auto& o : clients_) {
    if (o->id == from.id || !o->haveEndpoint || !o->link->linkUp(nowMs)) continue;
    uint8_t out[kNodeMaxDatagram];
    const size_t k = o->link->makePacket(wire, len, out, sizeof(out));
    if (!k) continue;
    net_.send(o->endpoint, out, k);
    o->packetsOut++;
    if (voice) stats_.voiceForwarded++;
    else stats_.textForwarded++;
  }
}

void ReflectorCore::tick(uint32_t nowMs) {
  for (auto& c : clients_) {
    if (!c->haveEndpoint) continue;
    if (nowMs - c->lastSeenMs >= kNodeDeadMs) {  // tace da troppo: si dimentica l'indirizzo (nessun datagramma verso un indirizzo che non risponde)
      c->haveEndpoint = false;
      if (c->wasUp) {
        c->wasUp = false;
        say(std::string("client ") + c->name + " non risponde piu'");
      }
      continue;
    }
    uint8_t out[kNodeMaxDatagram];
    const size_t m = c->link->maintenance(nowMs, out, sizeof(out));
    if (m) net_.send(c->endpoint, out, m);
    const bool up = c->link->linkUp(nowMs);
    if (up != c->wasUp) {
      c->wasUp = up;
      say(std::string("client ") + c->name + (up ? " collegato" : " non piu' collegato"));
    }
  }
  if (talking_ && nowMs - talkLastMs_ > kTalkHangMs) {
    talking_ = false;
    endTalk();
    say("trasmissione finita (nessun pacchetto)");
  }
}

std::vector<ReflectorCore::ClientInfo> ReflectorCore::clients(uint32_t nowMs) const {
  std::vector<ClientInfo> v;
  for (const auto& c : clients_) {
    ClientInfo i;
    i.id = c->id;
    i.name = c->name;
    i.haveEndpoint = c->haveEndpoint;
    i.endpoint = c->endpoint;
    i.up = c->link->linkUp(nowMs);
    i.lastSeenMs = c->lastSeenMs;
    i.packetsIn = c->packetsIn;
    i.packetsOut = c->packetsOut;
    i.declaredAddr = c->link->peerPublicAddress();
    v.push_back(i);
  }
  return v;
}

}  // namespace lcr
