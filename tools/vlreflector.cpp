// Reflector VoLoRa per un server (Linux): vedi docs/studio-reflector.md. I nodi e i ripetitori si collegano da dietro il loro router SENZA aprire
// porte: scrivono loro per primi e il reflector risponde all'indirizzo da cui e' arrivato il datagramma. Serve solo una porta UDP raggiungibile
// sul server.
//
//   make vlreflector
//   build/vlreflector --config reflector.conf [--port 5400] [--id 1000] [--voice-mode 4] [--status-every 300] [--duration SECONDI]
//
// File di configurazione, una riga per client (righe vuote e che iniziano con # si ignorano):
//   <id> <chiave: 32 cifre esadecimali> <nome>
//   2 000102030405060708090a0b0c0d0e0f nodo-firenze
// L'id e la chiave sono quelli che il client imposta con NODEID e NODEKEY; sul client PEERID e' l'id del reflector (--id).
// Un client si toglie cancellando la sua riga e riavviando il reflector. Solo IPv4 per ora.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#include "reflector_core.h"

using namespace lcr;

static volatile sig_atomic_t g_stop = 0;
static void onSignal(int) { g_stop = 1; }

static uint32_t nowMs() {
  using namespace std::chrono;
  return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
static uint32_t rngFn() { return static_cast<uint32_t>(rand()) ^ (static_cast<uint32_t>(rand()) << 16); }

static bool hexKey(const std::string& s, uint8_t* out) {
  if (s.size() != 32) return false;
  for (int i = 0; i < 32; i++) {
    const char c = s[static_cast<size_t>(i)];
    int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
    if (v < 0) return false;
    if (i % 2 == 0) out[i / 2] = static_cast<uint8_t>(v << 4);
    else out[i / 2] = static_cast<uint8_t>(out[i / 2] | v);
  }
  return true;
}

static std::string epText(Endpoint e) {
  char b[40];
  snprintf(b, sizeof(b), "%u.%u.%u.%u:%u", static_cast<unsigned>((e >> 40) & 255), static_cast<unsigned>((e >> 32) & 255), static_cast<unsigned>((e >> 24) & 255),
           static_cast<unsigned>((e >> 16) & 255), static_cast<unsigned>(e & 0xFFFF));
  return b;
}

static void logLine(const std::string& s) {
  const time_t t = time(nullptr);
  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));
  printf("%s %s\n", ts, s.c_str());
  fflush(stdout);
}

struct UdpNet : public IReflectorNet {
  int sock = -1;
  void send(Endpoint to, const uint8_t* d, size_t n) override {
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(static_cast<uint32_t>(to >> 16));
    a.sin_port = htons(static_cast<uint16_t>(to & 0xFFFF));
    sendto(sock, d, n, 0, reinterpret_cast<sockaddr*>(&a), sizeof(a));
  }
};

int main(int argc, char** argv) {
  std::string configPath;
  int port = 5400, duration = 0, statusEvery = 300;
  uint32_t reflectorId = 1000;
  int voiceMode = 4;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--config") configPath = next();
    else if (a == "--port") port = atoi(next());
    else if (a == "--id") reflectorId = static_cast<uint32_t>(strtoul(next(), nullptr, 10));
    else if (a == "--voice-mode") voiceMode = atoi(next());
    else if (a == "--status-every") statusEvery = atoi(next());
    else if (a == "--duration") duration = atoi(next());
    else {
      fprintf(stderr, "parametro sconosciuto: %s (vedi l'inizio di tools/vlreflector.cpp)\n", a.c_str());
      return 2;
    }
  }
  if (configPath.empty() || reflectorId == 0 || (voiceMode != 1 && voiceMode != 4) || port < 1 || port > 65535) {
    fprintf(stderr, "servono --config FILE; --id diverso da 0, --voice-mode 1 o 4, --port 1-65535\n");
    return 2;
  }
  srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid()));

  UdpNet net;
  ReflectorCore core(net, reflectorId, static_cast<uint8_t>(voiceMode), rngFn(), rngFn);
  core.setLog(logLine);
  std::ifstream in(configPath);
  if (!in) {
    fprintf(stderr, "non riesco ad aprire %s\n", configPath.c_str());
    return 2;
  }
  std::string line;
  int n = 0, lineNo = 0;
  while (std::getline(in, line)) {
    lineNo++;
    const size_t h = line.find('#');
    if (h != std::string::npos) line.resize(h);
    std::istringstream ss(line);
    std::string idText, keyText, name;
    if (!(ss >> idText)) continue;
    ss >> keyText >> name;
    uint8_t key[kNodeKeyBytes];
    const unsigned long id = strtoul(idText.c_str(), nullptr, 10);
    if (id == 0 || !hexKey(keyText, key) || !core.addClient(static_cast<uint32_t>(id), key, name.c_str())) {
      fprintf(stderr, "%s riga %d: attesi <id diverso da 0 e dal reflector, non ripetuto> <32 cifre esadecimali> <nome>\n", configPath.c_str(), lineNo);
      return 2;
    }
    n++;
  }
  if (n == 0) {
    fprintf(stderr, "nessun client in %s\n", configPath.c_str());
    return 2;
  }

  net.sock = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in me;
  memset(&me, 0, sizeof(me));
  me.sin_family = AF_INET;
  me.sin_addr.s_addr = htonl(INADDR_ANY);
  me.sin_port = htons(static_cast<uint16_t>(port));
  if (bind(net.sock, reinterpret_cast<sockaddr*>(&me), sizeof(me)) != 0) {
    perror("bind");
    return 1;
  }
  signal(SIGINT, onSignal);
  signal(SIGTERM, onSignal);
  char b[160];
  snprintf(b, sizeof(b), "vlreflector id %u in ascolto sulla porta UDP %d, modo voce %d, %d client in elenco", reflectorId, port, voiceMode, n);
  logLine(b);

  const uint32_t start = nowMs();
  uint32_t lastStatus = start;
  while (!g_stop) {
    const uint32_t t = nowMs();
    if (duration > 0 && t - start > static_cast<uint32_t>(duration) * 1000u) break;
    pollfd pfd = {net.sock, POLLIN, 0};
    if (poll(&pfd, 1, 20) > 0) {
      for (int guard = 0; guard < 64; guard++) {  // svuota quello che e' arrivato
        uint8_t buf[kNodeMaxDatagram + 16];
        sockaddr_in from;
        socklen_t fl = sizeof(from);
        const ssize_t k = recvfrom(net.sock, buf, sizeof(buf), MSG_DONTWAIT, reinterpret_cast<sockaddr*>(&from), &fl);
        if (k <= 0) break;
        const Endpoint ep = (static_cast<Endpoint>(ntohl(from.sin_addr.s_addr)) << 16) | ntohs(from.sin_port);
        core.onDatagram(ep, buf, static_cast<size_t>(k), nowMs());
      }
    }
    core.tick(nowMs());
    if (statusEvery > 0 && t - lastStatus >= static_cast<uint32_t>(statusEvery) * 1000u) {
      lastStatus = t;
      std::string s = "stato:";
      for (const auto& c : core.clients(t)) s += " " + c.name + (c.up ? "=su(" + epText(c.endpoint) + ")" : "=giu");
      logLine(s);
    }
  }
  const ReflectorCore::Stats& s = core.stats();
  snprintf(b, sizeof(b), "fine: datagrammi %u, sconosciuti %u, firme errate %u, ripetuti %u, voce inoltrata %u, testi %u, voce bloccata %u, trasmissioni %u",
           s.datagrams, s.unknownClient, s.badSignature, s.replays, s.voiceForwarded, s.textForwarded, s.voiceBlocked, s.talks);
  logLine(b);
  return 0;
}
