// lcr_decode: dai pacchetti LoRa catturati con un SDR (esadecimale, uno per riga) alle trasmissioni di VoLoRa.
//
//   make lcr_decode
//   build/lcr_decode cattura.txt [--out CARTELLA] [--accept-bad-crc]
//   ... | build/lcr_decode -            (legge dallo standard input)
//
// Per ogni sessione vocale scrive sessione_<id>.c2 (frame Codec2: 6 byte l'uno a 1200, 16 a 3200): si decodifica con
//   cp sessione_<id>.c2 sessione_<id>.bin   (c2dec 0.9.2 non decodifica tutto un .c2 senza intestazione)
//   c2dec 1200 sessione_<id>.bin sessione_<id>.raw
//   ffmpeg -f s16le -ar 8000 -ac 1 -i sessione_<id>.raw sessione_<id>.wav
// Per le prove di collegamento stampa l'esito (frame ok, persi, danneggiati), come la scheda ricevente.
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "lcr_decode_core.h"

using namespace lcr;
using namespace lcr::decode;

static void usage() {
  std::puts("uso: lcr_decode FILE|- [--out CARTELLA] [--accept-bad-crc]\n"
            "FILE: righe come \"t=1730900000.123 crc=1 hex=0004d2...\" (da tools/sdr/lora_rx.py)");
}

int main(int argc, char** argv) {
  std::string in, outDir = ".";
  bool acceptBad = false;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--out" && i + 1 < argc) {
      outDir = argv[++i];
    } else if (a == "--accept-bad-crc") {
      acceptBad = true;
    } else if (a == "-h" || a == "--help") {
      usage();
      return 0;
    } else if (in.empty()) {
      in = a;
    } else {
      usage();
      return 2;
    }
  }
  if (in.empty()) {
    usage();
    return 2;
  }
  std::string text;
  if (in == "-") {
    text.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
  } else {
    std::ifstream f(in);
    if (!f) {
      std::fprintf(stderr, "impossibile aprire %s\n", in.c_str());
      return 1;
    }
    text.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }

  Summary sum;
  const auto caps = parseCaptures(text, &sum);
  const auto sessions = reassemble(caps, &sum, acceptBad);

  std::printf("Righe lette: %zu, pacchetti: %zu (CRC errato scartati: %zu, non conformi al protocollo: %zu, altri modi: %zu)\n\n", sum.lines,
              caps.size(), sum.badCrc, sum.badPacket, sum.otherModes);
  if (sessions.empty()) std::puts("Nessuna trasmissione riconosciuta.");

  int voice = 0;
  for (const auto& s : sessions) {
    if (s.isTest) {
      std::printf("PROVA DI COLLEGAMENTO sessione %u da %s: ok %u/%u (%d%%), danneggiati %u, persi %u, pacchetti recuperati %u%s\n", s.id,
                  s.callsign.empty() ? "-" : s.callsign.c_str(), s.test.framesOk, s.test.framesOut, s.test.percentOk(), s.test.framesBad,
                  s.test.framesLost, s.recovered, s.test.complete() ? "" : " [parziale]");
      continue;
    }
    const double sec = s.c2.size() / static_cast<double>(s.frameBytes) * kFrameMs / 1000.0;
    std::printf("VOCE sessione %u da %s: %u pacchetti, %u frame (%.1f s), persi %u, pacchetti recuperati %u%s\n", s.id,
                s.callsign.empty() ? "-" : s.callsign.c_str(), s.packets, s.frames, sec, s.framesLost, s.recovered,
                s.ended ? "" : " [cattura interrotta]");
    if (s.c2.empty()) continue;
    char name[96];
    std::snprintf(name, sizeof(name), "%s/sessione_%u.c2", outDir.c_str(), s.id);
    std::ofstream o(name, std::ios::binary);
    o.write(reinterpret_cast<const char*>(s.c2.data()), static_cast<std::streamsize>(s.c2.size()));
    std::printf("  scritto %s\n  cp %s %s/sessione_%u.bin && c2dec %d %s/sessione_%u.bin %s/sessione_%u.raw && ffmpeg -f s16le -ar 8000 -ac 1 -i %s/sessione_%u.raw %s/sessione_%u.wav\n",
                name, name, outDir.c_str(), s.id, s.mode == kModeCodec2_3200 ? 3200 : 1200, outDir.c_str(), s.id, outDir.c_str(), s.id, outDir.c_str(), s.id, outDir.c_str(), s.id);
    voice++;
  }
  for (const auto& r : sum.replies) {
    std::printf("RISPOSTA di %s alla prova %u: ok %u/%u, danneggiati %u, persi %u, recuperati %u, RSSI %d dBm\n", r.callsign.c_str(), r.session,
                r.report.framesOk, r.report.framesOut, r.report.framesBad, r.report.framesLost, r.report.recovered, r.report.rssiDbm);
  }
  (void)voice;
  return 0;
}
