// Il reflector: client finti (veri NodeLink) collegati a un ReflectorCore con una rete finta che ricorda da dove arriva ogni datagramma.
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "node_link.h"
#include "packet.h"
#include "protocol.h"
#include "reflector_core.h"
#include "testing.h"

using namespace lcr;

static uint32_t g_rng = 7;
static uint32_t rngFn() { return g_rng += 7919; }

typedef std::vector<uint8_t> Bytes;
static const uint32_t kRefId = 900;

struct Net : public IReflectorNet {
  std::map<Endpoint, std::deque<Bytes>> toEp;  // datagrammi del reflector verso un indirizzo
  int sent = 0;
  void send(Endpoint to, const uint8_t* d, size_t n) override {
    toEp[to].push_back(Bytes(d, d + n));
    sent++;
  }
};

struct Client {
  uint32_t id;
  Endpoint ep;
  std::unique_ptr<NodeLink> link;
  std::vector<Packet> received;  // pacchetti radio arrivati dal reflector
  uint32_t boot;
  uint8_t key[kNodeKeyBytes];
  Client(uint32_t id_, Endpoint ep_, uint8_t keyByte, uint32_t boot_) : id(id_), ep(ep_), boot(boot_) {
    memset(key, keyByte, sizeof(key));
    reset(boot_);
  }
  void reset(uint32_t boot_) {
    NodeLink::Config c;
    c.nodeId = id;
    c.peerId = kRefId;
    memcpy(c.key, key, sizeof(key));
    link.reset(new NodeLink(c, boot_, rngFn));
    received.clear();
  }
};

struct Rig {
  Net net;
  ReflectorCore core;
  std::vector<std::unique_ptr<Client>> cl;
  uint32_t t = 1000;
  std::vector<std::string> log;
  explicit Rig(uint8_t voiceMode = kModeCodec2_3200) : core(net, kRefId, voiceMode, 0x5000, rngFn) {
    core.setLog([this](const std::string& s) { log.push_back(s); });
  }
  Client& add(uint32_t id, uint8_t keyByte, Endpoint ep, bool inCore = true) {
    cl.emplace_back(new Client(id, ep, keyByte, 0xC000 + id));
    if (inCore) {
      uint8_t k[kNodeKeyBytes];
      memset(k, keyByte, sizeof(k));
      core.addClient(id, k, ("c" + std::to_string(id)).c_str());
    }
    return *cl.back();
  }
  // Un giro: ogni client manda quello che ha da mandare (HELLO/KEEPALIVE), il reflector fa la manutenzione, e si smistano i datagrammi
  void step() {
    uint8_t out[kNodeMaxDatagram];
    for (auto& c : cl) {
      const size_t m = c->link->maintenance(t, out, sizeof(out));
      if (m) core.onDatagram(c->ep, out, m, t);
    }
    core.tick(t);
    pump();
  }
  void pump() {
    for (int round = 0; round < 8; round++) {
      bool any = false;
      for (auto& c : cl) {
        auto& q = net.toEp[c->ep];
        while (!q.empty()) {
          any = true;
          const Bytes d = q.front();
          q.pop_front();
          NodeLink::Rx& rx = c->link->onDatagram(d.data(), d.size(), t);
          for (size_t i = 0; i < rx.nReplies; i++) core.onDatagram(c->ep, rx.replies[i].data, rx.replies[i].len, t);
          if (rx.kind == NodeLink::Kind::Packet) {
            Packet p;
            if (decodePacket(rx.packet, rx.packetLen, p)) c->received.push_back(p);
          }
        }
      }
      if (!any) break;
    }
  }
  void run(uint32_t ms, uint32_t stepMs = 100) {
    const uint32_t end = t + ms;
    while (t < end) {
      step();
      t += stepMs;
    }
  }
  void sendPacket(Client& c, const Packet& p) {
    uint8_t wire[kMaxWireBytes], out[kNodeMaxDatagram];
    const size_t w = encodePacket(p, wire, sizeof(wire));
    const size_t k = c.link->makePacket(wire, w, out, sizeof(out));
    core.onDatagram(c.ep, out, k, t);
    pump();
  }
  bool up(uint32_t id) {
    for (auto& i : core.clients(t))
      if (i.id == id) return i.up;
    return false;
  }
};

static Packet voicePacket(uint8_t mode, uint16_t session, uint8_t seq, uint8_t flags, const char* call = "IU5HJU") {
  Packet p;
  p.mode = mode;
  p.session = session;
  p.seq = seq;
  p.flags = flags;
  if (call && (flags & kFlagStart)) strncpy(p.callsign, call, kMaxCallsign);
  p.payloadLen = static_cast<uint8_t>(frameBytesFor(mode) * 2);
  for (size_t i = 0; i < p.payloadLen; i++) p.payload[i] = static_cast<uint8_t>(seq * 3 + i);
  return p;
}

static Packet textPacket(const char* call, const char* text) {
  Packet p;
  p.mode = kModeText;
  p.session = 0x1234;
  p.flags = kFlagStart | kFlagEnd;
  strncpy(p.callsign, call, kMaxCallsign);
  p.payloadLen = static_cast<uint8_t>(strlen(text));
  memcpy(p.payload, text, p.payloadLen);
  return p;
}

static void testHandshake() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  CHECK(!r.up(1) && !r.up(2));
  r.run(4000);
  CHECK(r.up(1) && r.up(2));
  CHECK(a.link->linkUp(r.t) && b.link->linkUp(r.t));  // anche i client vedono il reflector attivo
  const auto info = r.core.clients(r.t);
  CHECK(info.size() == 2 && info[0].haveEndpoint && info[0].endpoint == 5001 && info[1].endpoint == 5002);
  CHECK(r.log.size() >= 2 && r.log[0].find("collegato") != std::string::npos);
}

static void testTextAndRelayed() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  Client& c = r.add(3, 0xC3, 5003);
  r.run(4000);
  r.sendPacket(a, textPacket("IU5HJU", "ciao a tutti"));
  CHECK(a.received.empty());  // non torna a chi l'ha mandato
  CHECK(b.received.size() == 1 && c.received.size() == 1);
  CHECK(b.received[0].mode == kModeText && (b.received[0].flags & kFlagRelayed) && strcmp(b.received[0].callsign, "IU5HJU") == 0);
  CHECK(b.received[0].payloadLen == 12 && memcmp(b.received[0].payload, "ciao a tutti", 12) == 0);
  CHECK(r.core.stats().textForwarded == 2);
  // il testo passa sempre, anche mentre qualcuno parla
  r.sendPacket(b, voicePacket(kModeCodec2_3200, 0x0101, 0, kFlagStart));
  r.sendPacket(c, textPacket("IZ0ABC", "messaggio"));
  CHECK(a.received.size() == 2 && a.received.back().mode == kModeText);
}

static void testVoiceLockAndRelease() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  Client& c = r.add(3, 0xC3, 5003);
  r.run(4000);
  // A parla: B e C ricevono, A no
  r.sendPacket(a, voicePacket(kModeCodec2_3200, 0x0A0A, 0, kFlagStart));
  r.sendPacket(a, voicePacket(kModeCodec2_3200, 0x0A0A, 1, 0));
  CHECK(b.received.size() == 2 && c.received.size() == 2 && a.received.empty());
  CHECK((b.received[0].flags & kFlagRelayed) && b.received[0].mode == kModeCodec2_3200);
  CHECK(r.core.talker(r.t) == 1);
  // B prova a parlare sopra A: scartato
  r.t += 500;
  r.sendPacket(b, voicePacket(kModeCodec2_3200, 0x0B0B, 0, kFlagStart, "IZ0ABC"));
  CHECK(a.received.empty() && c.received.size() == 2 && r.core.stats().voiceBlocked == 1);
  CHECK(r.core.talker(r.t) == 1);
  // A continua e finisce (END): il canale e' libero subito
  r.sendPacket(a, voicePacket(kModeCodec2_3200, 0x0A0A, 2, kFlagEnd));
  CHECK(c.received.size() == 3 && r.core.talker(r.t) == 0);
  r.sendPacket(b, voicePacket(kModeCodec2_3200, 0x0B0B, 0, kFlagStart, "IZ0ABC"));
  CHECK(a.received.size() == 1 && c.received.size() == 4 && r.core.talker(r.t) == 2);
  CHECK(r.core.stats().talks == 2);
  // B tace senza END: dopo kTalkHangMs il canale si libera, e C puo' parlare
  r.t += ReflectorCore::kTalkHangMs + 200;
  r.core.tick(r.t);
  CHECK(r.core.talker(r.t) == 0);
  r.sendPacket(c, voicePacket(kModeCodec2_3200, 0x0C0C, 0, kFlagStart, "IW0XYZ"));
  CHECK(a.received.size() == 2 && b.received.size() > 0 && r.core.talker(r.t) == 3);
  // la parita' dopo il pacchetto di fine non inizia una trasmissione ne' tiene il canale, ma passa ai client
  r.sendPacket(c, voicePacket(kModeCodec2_3200, 0x0D0D, 1, kFlagEnd));
  CHECK(r.core.talker(r.t) == 0);  // con il pacchetto di fine il canale e' libero subito
  const uint32_t talks = r.core.stats().talks;
  const size_t got = a.received.size();
  Packet par;
  par.mode = kModeCodec2_3200;
  par.session = 0x0D0D;
  par.seq = 3;
  par.flags = kFlagParity;
  par.payloadLen = static_cast<uint8_t>(kParityHeader + 16);
  par.payload[0] = 2;  // pacchetti del gruppo
  par.payload[1] = 0;  // XOR dei conteggi
  r.sendPacket(c, par);
  CHECK(r.core.stats().talks == talks && r.core.talker(r.t) == 0 && a.received.size() == got + 1 && (a.received.back().flags & kFlagParity));
  // lo stesso client puo' ricominciare con un'altra sessione senza aspettare
  r.sendPacket(c, voicePacket(kModeCodec2_3200, 0x0D0D, 0, kFlagStart, "IW0XYZ"));
  CHECK(r.core.stats().talks == talks + 1);
}

static void testModesFiltered() {
  Rig r;  // modo voce 4
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  r.run(4000);
  r.sendPacket(a, voicePacket(kModeCodec2_1200, 0x0101, 0, kFlagStart));  // 1200: non e' il modo del reflector
  CHECK(b.received.empty() && r.core.stats().badMode == 1);
  Packet busy = makeBusyPacket(0x77, "IR5XXX", 30);
  r.sendPacket(a, busy);
  CHECK(b.received.empty() && r.core.stats().badMode == 2);
  // un reflector a 1200 lascia passare il 1200 e non il 3200
  Rig q(kModeCodec2_1200);
  Client& c = q.add(1, 0xA1, 5001);
  Client& d = q.add(2, 0xB2, 5002);
  q.run(4000);
  q.sendPacket(c, voicePacket(kModeCodec2_3200, 0x0202, 0, kFlagStart));
  CHECK(d.received.empty() && q.core.stats().badMode == 1);
  q.sendPacket(c, voicePacket(kModeCodec2_1200, 0x0203, 0, kFlagStart));
  CHECK(d.received.size() == 1);
}

static void testUnknownAndBadKey() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  r.run(4000);
  // un id sconosciuto: niente risposta (il reflector non si puo' usare per amplificare)
  Client& ghost = r.add(77, 0x77, 6001, false);
  r.run(2000);
  CHECK(r.net.toEp[6001].empty() && r.core.stats().unknownClient > 0);
  // id noto ma chiave sbagliata: firma errata, nessuna risposta
  Client& bad = r.add(3, 0x11, 6002, false);
  uint8_t k3[kNodeKeyBytes];
  memset(k3, 0x33, sizeof(k3));
  r.core.addClient(3, k3, "c3");
  r.run(2000);
  CHECK(r.net.toEp[6002].empty() && r.core.stats().badSignature > 0 && !r.up(3));
  CHECK(r.up(1) && r.up(2));
  (void)a;
  (void)b;
  (void)ghost;
  (void)bad;
  // addClient: id 0, id del reflector, doppio
  CHECK(!r.core.addClient(0, k3, "x") && !r.core.addClient(kRefId, k3, "x") && !r.core.addClient(1, k3, "x"));
}

static void testReplayFromAnotherAddress() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  r.run(4000);
  // si cattura un datagramma PACKET di A
  uint8_t wire[kMaxWireBytes], out[kNodeMaxDatagram];
  const size_t w = encodePacket(textPacket("IU5HJU", "uno"), wire, sizeof(wire));
  const size_t k = a.link->makePacket(wire, w, out, sizeof(out));
  r.core.onDatagram(5001, out, k, r.t);
  r.pump();
  CHECK(b.received.size() == 1);
  // lo riascolta e lo rimanda da un altro indirizzo: contatore gia' visto, non e' inoltrato e l'indirizzo di A non cambia
  const uint32_t replays = r.core.stats().replays;
  r.core.onDatagram(7777, out, k, r.t);
  r.pump();
  CHECK(r.core.stats().replays == replays + 1 && b.received.size() == 1);
  CHECK(r.core.clients(r.t)[0].endpoint == 5001);
}

static void testNatRebind() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  r.run(4000);
  // il NAT cambia la porta di A: il suo prossimo datagramma firmato valido (KEEPALIVE) arriva dalla nuova e l'indirizzo si aggiorna
  a.ep = 5555;
  r.run(25000);
  CHECK(r.core.clients(r.t)[0].endpoint == 5555 && r.up(1));
  r.sendPacket(b, textPacket("IZ0ABC", "dopo il cambio"));
  CHECK(a.received.size() == 1);  // arriva alla nuova porta
}

static void testDeadClientAndRestart() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  r.run(4000);
  CHECK(r.up(1) && r.up(2));
  // B sparisce (nessun datagramma): dopo kNodeDeadMs il reflector lo dimentica e non gli manda piu' niente
  uint32_t last = r.t;
  while (r.t < last + 70000) {
    uint8_t out[kNodeMaxDatagram];
    const size_t m = a.link->maintenance(r.t, out, sizeof(out));  // solo A resta in linea
    if (m) r.core.onDatagram(a.ep, out, m, r.t);
    r.core.tick(r.t);
    auto& qa = r.net.toEp[a.ep];
    while (!qa.empty()) {
      const Bytes d = qa.front();
      qa.pop_front();
      NodeLink::Rx& rx = a.link->onDatagram(d.data(), d.size(), r.t);
      for (size_t i = 0; i < rx.nReplies; i++) r.core.onDatagram(a.ep, rx.replies[i].data, rx.replies[i].len, r.t);
    }
    r.net.toEp[b.ep].clear();
    r.t += 100;
  }
  CHECK(!r.core.clients(r.t)[1].haveEndpoint && !r.up(2) && r.up(1));
  const int sent = r.net.sent;
  r.core.tick(r.t + 100);
  CHECK(r.net.sent == sent || !r.core.clients(r.t)[1].haveEndpoint);
  // B torna con un altro id di avvio (riavviato): si riautentica
  b.reset(0xFFFF);
  r.run(6000);
  CHECK(r.up(2) && r.core.clients(r.t)[1].haveEndpoint);
  r.sendPacket(a, textPacket("IU5HJU", "bentornato"));
  CHECK(b.received.size() == 1);
}

static void testPacketCounts() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  r.add(3, 0xC3, 5003);
  r.run(4000);
  for (int i = 0; i < 5; i++) r.sendPacket(a, textPacket("IU5HJU", "x"));
  const auto info = r.core.clients(r.t);
  CHECK(info[0].packetsIn == 5 && info[1].packetsOut == 5 && info[2].packetsOut == 5 && info[0].packetsOut == 0);
  CHECK(b.received.size() == 5);
  // un client non ancora collegato non riceve niente
  Rig q;
  Client& c = q.add(1, 0xA1, 5001);
  uint8_t kb[kNodeKeyBytes];
  memset(kb, 0xB2, sizeof(kb));
  q.core.addClient(2, kb, "muto");  // in elenco, ma non fa mai niente: non e' collegato
  q.run(4000);
  q.sendPacket(c, textPacket("IU5HJU", "solo"));
  CHECK(q.core.stats().textForwarded == 0);
}

static void testRegistryOperations() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  r.run(4000);
  CHECK(r.core.idByName("c1") == 1 && r.core.idByName("C2") == 2 && r.core.idByName("x") == 0 && r.core.idByName("") == 0 && r.core.hasClient(2) && !r.core.hasClient(9));
  // cambiare la chiave: il client con la chiave vecchia non e' piu' accettato (firma errata), e con la nuova si ricollega
  uint8_t nk[kNodeKeyBytes];
  memset(nk, 0xEE, sizeof(nk));
  CHECK(r.core.setClientKey(1, nk) && !r.core.setClientKey(9, nk));
  CHECK(!r.up(1) && !r.core.clients(r.t)[0].haveEndpoint);
  const uint32_t bad = r.core.stats().badSignature;
  r.run(25000);  // il suo prossimo KEEPALIVE
  CHECK(!r.up(1) && r.core.stats().badSignature > bad);  // A insiste con la chiave vecchia
  CHECK(r.up(2));                                          // gli altri non si accorgono di niente
  memset(a.key, 0xEE, sizeof(a.key));
  a.reset(0xAAAA);
  r.run(6000);
  CHECK(r.up(1));
  r.sendPacket(a, textPacket("IU5HJU", "con la chiave nuova"));
  CHECK(b.received.size() == 1);
  // il nome si cambia
  CHECK(r.core.setClientName(1, "IU5HJU") && r.core.idByName("iu5hju") == 1 && r.core.idByName("c1") == 0);
  // togliere un client: sparisce, non riceve piu' e non e' piu' in elenco; chi parlava libera il canale
  r.sendPacket(b, voicePacket(kModeCodec2_3200, 0x0101, 0, kFlagStart));
  CHECK(r.core.talker(r.t) == 2);
  CHECK(r.core.removeClient(2) && !r.core.removeClient(2) && !r.core.hasClient(2));
  CHECK(r.core.talker(r.t) == 0 && r.core.clients(r.t).size() == 1);
  const size_t got = a.received.size();
  r.sendPacket(a, textPacket("IU5HJU", "da solo"));
  CHECK(a.received.size() == got && r.core.stats().textForwarded == 1 + 0);
  r.core.onDatagram(5002, nullptr, 0, r.t);  // e un datagramma di chi non c'e' piu' si ignora
  CHECK(r.core.stats().unknownClient > 0);
}

static void testTalkHistory() {
  Rig r;
  Client& a = r.add(1, 0xA1, 5001);
  Client& b = r.add(2, 0xB2, 5002);
  r.add(3, 0xC3, 5003);
  r.run(4000);
  CHECK(!r.core.currentTalk(r.t).active && r.core.recentTalks().empty());
  r.sendPacket(a, voicePacket(kModeCodec2_3200, 0x0A0A, 0, kFlagStart, "IZ5IOF"));
  r.t += 400;
  r.sendPacket(a, voicePacket(kModeCodec2_3200, 0x0A0A, 1, 0));
  ReflectorCore::Talk t = r.core.currentTalk(r.t);
  CHECK(t.active && t.clientId == 1 && t.client == "c1" && t.callsign == "IZ5IOF" && t.packets == 2 && t.lastMs - t.startMs == 400);
  // un'altra voce mentre parla A e' scartata: non entra nella trasmissione ne' nella storia
  r.sendPacket(b, voicePacket(kModeCodec2_3200, 0x0B0B, 0, kFlagStart, "IW0XYZ"));
  CHECK(r.core.currentTalk(r.t).packets == 2 && r.core.currentTalk(r.t).callsign == "IZ5IOF" && r.core.recentTalks().empty());
  r.t += 400;
  r.sendPacket(a, voicePacket(kModeCodec2_3200, 0x0A0A, 2, kFlagEnd));
  CHECK(!r.core.currentTalk(r.t).active && r.core.recentTalks().size() == 1);
  const ReflectorCore::Talk& done = r.core.recentTalks()[0];
  CHECK(!done.active && done.callsign == "IZ5IOF" && done.client == "c1" && done.packets == 3 && done.lastMs - done.startMs == 800);
  // una che finisce per silenzio entra nella storia al giro di manutenzione, e la piu' recente e' la prima
  r.sendPacket(b, voicePacket(kModeCodec2_3200, 0x0C0C, 0, kFlagStart, "IW0XYZ"));
  r.t += ReflectorCore::kTalkHangMs + 100;
  CHECK(!r.core.currentTalk(r.t).active);
  r.core.tick(r.t);
  CHECK(r.core.recentTalks().size() == 2 && r.core.recentTalks()[0].callsign == "IW0XYZ" && r.core.recentTalks()[1].callsign == "IZ5IOF");
  // senza nominativo nel primo pacchetto: "-", e poi arriva
  r.sendPacket(a, voicePacket(kModeCodec2_3200, 0x0D0D, 1, 0));
  CHECK(r.core.currentTalk(r.t).callsign == "-");
  r.sendPacket(a, voicePacket(kModeCodec2_3200, 0x0D0D, 0, kFlagStart, "IZ5IOF"));  // stessa sessione: il nominativo si completa
  CHECK(r.core.currentTalk(r.t).callsign == "IZ5IOF" && r.core.recentTalks().size() == 2);
  // al massimo kRecentTalks
  for (int i = 0; i < 30; i++) {
    r.t += 100;
    r.sendPacket(a, voicePacket(kModeCodec2_3200, static_cast<uint16_t>(0x2000 + i), 0, kFlagStart, "IZ5IOF"));
    r.sendPacket(a, voicePacket(kModeCodec2_3200, static_cast<uint16_t>(0x2000 + i), 1, kFlagEnd));
  }
  CHECK(r.core.recentTalks().size() == ReflectorCore::kRecentTalks);
}

// L'indirizzo pubblico dichiarato dal nodo: il reflector lo mostra accanto a quello che vede (che dietro un router e' quello del router).
static void testDeclaredAddress() {
  Rig r;
  Client& a = r.add(1, 0x11, 0xC0A8FE01u * 65536ull + 4000);  // il reflector vede 192.168.254.1: un indirizzo interno messo dal router
  Client& b = r.add(2, 0x22, 5002);
  (void)b;
  r.run(3000);
  CHECK(r.up(1) && r.up(2));
  CHECK(r.core.clients(r.t)[0].declaredAddr == 0 && r.core.clients(r.t)[1].declaredAddr == 0);  // nessuno lo dichiara
  a.link->setPublicAddress((78u << 24) | (211u << 16) | (227u << 8) | 119u);
  r.run(25000);  // un KEEPALIVE ogni 20 s
  const auto info = r.core.clients(r.t);
  CHECK(info[0].declaredAddr == ((78u << 24) | (211u << 16) | (227u << 8) | 119u));
  CHECK(info[0].endpoint == 0xC0A8FE01u * 65536ull + 4000);  // quello visto dal server resta quello da cui arrivano i pacchetti
  CHECK(info[1].declaredAddr == 0);
  // dopo un riavvio senza indirizzo il vecchio non resta
  a.reset(0xD000);
  r.run(5000);
  CHECK(r.core.clients(r.t)[0].declaredAddr == 0);
}

static void testPublicIpv4() {
  auto ip = [](uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return (a << 24) | (b << 16) | (c << 8) | d; };
  CHECK(isPublicIpv4(ip(78, 211, 227, 119)) && isPublicIpv4(ip(81, 56, 162, 206)) && isPublicIpv4(ip(8, 8, 8, 8)));
  CHECK(!isPublicIpv4(ip(192, 168, 1, 254)) && !isPublicIpv4(ip(192, 168, 0, 1)) && !isPublicIpv4(ip(10, 1, 2, 3)) && !isPublicIpv4(ip(127, 0, 0, 1)));
  CHECK(!isPublicIpv4(ip(172, 16, 0, 1)) && !isPublicIpv4(ip(172, 31, 255, 254)) && isPublicIpv4(ip(172, 15, 0, 1)) && isPublicIpv4(ip(172, 32, 0, 1)));
  CHECK(!isPublicIpv4(ip(169, 254, 1, 1)) && !isPublicIpv4(ip(100, 64, 0, 1)) && !isPublicIpv4(ip(100, 127, 255, 1)) && isPublicIpv4(ip(100, 63, 0, 1)) && isPublicIpv4(ip(100, 128, 0, 1)));
  CHECK(!isPublicIpv4(0) && !isPublicIpv4(ip(224, 0, 0, 1)) && !isPublicIpv4(ip(255, 255, 255, 255)));
}

int main() {
  testHandshake();
  testTextAndRelayed();
  testVoiceLockAndRelease();
  testModesFiltered();
  testUnknownAndBadKey();
  testReplayFromAnotherAddress();
  testNatRebind();
  testDeadClientAndRestart();
  testPacketCounts();
  testRegistryOperations();
  testTalkHistory();
  testDeclaredAddress();
  testPublicIpv4();
  TEST_MAIN_END();
}
