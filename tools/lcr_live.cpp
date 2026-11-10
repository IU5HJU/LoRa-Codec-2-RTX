// lcr_live: ricezione in tempo reale dai pacchetti catturati con un SDR (vedi tools/lcr_live_core.h).
//
//   make lcr_live
//   tail -n0 -F cattura.txt | build/lcr_live | c2dec 1200 - -   (a 3200: c2dec 3200 - -) | aplay -q -t raw -f S16_LE -r 8000 -c 1
//
// Legge dallo standard input le righe di tools/sdr/lora_rx.py ("t=... crc=1 hex=...") man mano che arrivano e scrive sullo
// standard output i frame Codec2 (6 byte l'uno a 1200, 16 a 3200) a ritmo reale, uno ogni 40 ms. Le righe di stato (nominativo, posizione,
// avanzamento, fine) vanno sullo standard error, cosi' non si mescolano con l'audio.
//   --messaggio        ascolta il profilo Messaggio (SF12): la riproduzione parte a messaggio completo
//   --accept-bad-crc   non scarta i pacchetti con CRC errato
//   --anticipo MS      cuscinetto per chi riproduce: i primi frame escono MS millisecondi in anticipo (predefinito 240, 0 = ritmo
//                      reale esatto); evita i vuoti ("underrun") di aplay
// Alla fine dell'ingresso aspetta che la trasmissione in corso finisca (al massimo 90 s) e termina.
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

#include "lcr_live_core.h"

using namespace lcr;
using namespace lcr::decode;

namespace {

class StdSink : public LiveRx::Sink {
 public:
  void frame(const uint8_t* f, size_t n, bool) override {
    std::fwrite(f, 1, n, stdout);
    std::fflush(stdout);  // a ogni frame: l'audio non deve aspettare che si riempia un buffer
  }
  void info(const std::string& line) override {
    std::fprintf(stderr, "[%s] %s\n", stamp().c_str(), line.c_str());
  }

 private:
  static std::string stamp() {
    const std::time_t t = std::time(nullptr);
    char b[16];
    std::strftime(b, sizeof(b), "%H:%M:%S", std::localtime(&t));
    return b;
  }
};

}  // namespace

int main(int argc, char** argv) {
  bool message = false, quality = false, acceptBad = false;
  uint32_t lead = 240;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--messaggio") {
      message = true;
    } else if (a == "--qualita") {
      quality = true;  // solo per l'avviso iniziale: il modo si riconosce dai pacchetti (1200 o 3200)
    } else if (a == "--accept-bad-crc") {
      acceptBad = true;
    } else if (a == "--anticipo" && i + 1 < argc) {
      lead = static_cast<uint32_t>(std::atoi(argv[++i]));
      if (lead > 1000) lead = 1000;
    } else {
      std::fprintf(stderr, "uso: lcr_live [--messaggio] [--qualita] [--accept-bad-crc] [--anticipo MS]   (legge le righe di lora_rx.py dallo standard input)\n");
      return a == "-h" || a == "--help" ? 0 : 2;
    }
  }
  StdSink sink;
  LiveRx live(sink, message, acceptBad, lead);
  const auto t0 = std::chrono::steady_clock::now();
  auto nowMs = [&]() {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
  };

  std::string buf;
  bool eof = false;
  uint32_t eofAt = 0;
  std::fprintf(stderr, "lcr_live: in ascolto (%s). Ctrl+C per fermare.\n", message ? "profilo Messaggio" : (quality ? "profilo Qualita'" : "profilo Standard"));
  for (;;) {
    if (!eof) {
      pollfd p{0, POLLIN, 0};
      const int r = poll(&p, 1, 10);
      if (r > 0) {
        char tmp[4096];
        const ssize_t n = read(0, tmp, sizeof(tmp));
        if (n > 0) {
          buf.append(tmp, static_cast<size_t>(n));
          size_t nl;
          while ((nl = buf.find('\n')) != std::string::npos) {
            Capture c;
            if (parseLine(buf.substr(0, nl), c)) live.feed(c, nowMs());
            buf.erase(0, nl + 1);
          }
        } else if (n == 0 || (n < 0 && errno != EINTR)) {
          eof = true;
          eofAt = nowMs();
        }
      }
    } else {
      usleep(10000);
      if (!live.receiving() || nowMs() - eofAt > 90000) break;
    }
    live.tick(nowMs());
  }
  return 0;
}
