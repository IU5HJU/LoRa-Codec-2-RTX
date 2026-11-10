// codec_compare: confronto dei modi del Codec2 sulla stessa registrazione, con la CATENA AUDIO DELLA SCHEDA.
//
// Le schede non danno al Codec2 l'audio grezzo del microfono: lo passano da un filtro passa-alto a 300 Hz, dalla
// decimazione a 8 kHz e da un AGC che porta la voce a circa -24 dBFS (lib/voicecore/audio_frontend.*). Questo strumento
// applica la STESSA catena (lo stesso codice) e poi codifica e decodifica con gli STESSI sorgenti del Codec2 delle schede
// (third_party/esp32_codec2-1.0.7): quello che si sente e' quello che si sentirebbe dalla scheda, a parte il microfono.
//
//   make codec_compare
//   arecord -f S16_LE -r 16000 -c 1 -d 10 voce.wav           (meglio: tools/confronta-codec.sh)
//   build/codec_compare voce.wav confronto
//   build/codec_compare voce.wav confronto --modi 3200,1200,700C,450 --perdita 5
//
// Ingresso: WAV PCM a 16 bit, mono, a 16000 Hz (come il microfono della scheda) oppure a 8000 Hz (si porta a 16 kHz).
//   Altri formati: ffmpeg -i in.wav -ar 16000 -ac 1 -sample_fmt s16 voce.wav
// Uscita: originale.wav (la registrazione), ingresso-scheda.wav (quello che il codec riceve dopo filtro e AGC) e un file
//   per ogni modo (3200.wav, 1200.wav, ...), questi ultimi a 8 kHz.
//   --perdita N   butta via il N% dei frame a caso (frame a zero, come la ricezione quando un pacchetto non si recupera)
//   --fondo DB    fondo del microfono per la soglia dell'AGC (predefinito -60, come la scheda)
//   --grezzo      senza AGC e con il passa-alto a 20 Hz (per sentire cosa succede senza la catena della scheda)
//   --seme N      seme della perdita casuale
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <sys/stat.h>

#include "audio_frontend.h"
#include "settings.h"

extern "C" {
#include "codec2.h"
void *codec2_malloc(size_t n) { return malloc(n); }
void codec2_free(void *p) { free(p); }
}

namespace {

double nowS() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return static_cast<double>(t.tv_sec) + static_cast<double>(t.tv_nsec) * 1e-9;
}

const struct {
  const char *name;
  int mode;
} kModes[] = {
    {"3200", CODEC2_MODE_3200}, {"2400", CODEC2_MODE_2400}, {"1600", CODEC2_MODE_1600}, {"1400", CODEC2_MODE_1400},
    {"1300", CODEC2_MODE_1300}, {"1200", CODEC2_MODE_1200}, {"700C", CODEC2_MODE_700C}, {"450", CODEC2_MODE_450},
};
constexpr size_t kNModes = sizeof(kModes) / sizeof(kModes[0]);

uint32_t rd32(const unsigned char *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint16_t rd16(const unsigned char *p) { return uint16_t(p[0] | p[1] << 8); }

// Legge un WAV mono a 16 bit, a 8000 o 16000 Hz. rate: la frequenza trovata.
bool loadWav(const char *path, std::vector<int16_t> &out, uint32_t &rate, std::string &err) {
  FILE *f = fopen(path, "rb");
  if (!f) { err = "impossibile aprire il file"; return false; }
  fseek(f, 0, SEEK_END);
  const long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size < 44 || size > 400L * 1000 * 1000) { fclose(f); err = "file vuoto, troppo piccolo o troppo grande"; return false; }
  std::vector<unsigned char> buf(static_cast<size_t>(size));
  if (fread(buf.data(), 1, buf.size(), f) != buf.size()) { fclose(f); err = "lettura non riuscita"; return false; }
  fclose(f);
  if (memcmp(buf.data(), "RIFF", 4) || memcmp(buf.data() + 8, "WAVE", 4)) { err = "non e' un file WAV"; return false; }
  size_t pos = 12;
  bool fmtOk = false;
  const unsigned char *data = nullptr;
  size_t bytes = 0;
  while (pos + 8 <= buf.size()) {
    const uint32_t len = rd32(&buf[pos + 4]);
    if (!memcmp(&buf[pos], "fmt ", 4) && pos + 8 + 16 <= buf.size()) {
      const uint16_t tag = rd16(&buf[pos + 8]), ch = rd16(&buf[pos + 10]), bits = rd16(&buf[pos + 22]);
      rate = rd32(&buf[pos + 12]);
      if (tag != 1 || ch != 1 || bits != 16 || (rate != 8000 && rate != 16000)) {
        err = "serve un WAV PCM a 16 bit, mono, a 16000 Hz oppure 8000 Hz (ffmpeg -i in.wav -ar 16000 -ac 1 -sample_fmt s16 voce.wav)";
        return false;
      }
      fmtOk = true;
    } else if (!memcmp(&buf[pos], "data", 4)) {
      data = &buf[pos + 8];
      bytes = len;
      if (pos + 8 + bytes > buf.size()) bytes = buf.size() - pos - 8;
      break;
    }
    pos += 8 + len + (len & 1);
  }
  if (!fmtOk || !data) { err = "WAV senza intestazione fmt o senza dati"; return false; }
  out.resize(bytes / 2);
  for (size_t i = 0; i < out.size(); i++) out[i] = static_cast<int16_t>(data[2 * i] | data[2 * i + 1] << 8);
  return true;
}

bool saveWav(const std::string &path, const int16_t *s, size_t n, uint32_t rate) {
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) return false;
  unsigned char h[44];
  memcpy(h, "RIFF\0\0\0\0WAVEfmt \x10\0\0\0\1\0\1\0", 24);
  auto w32 = [&](size_t o, uint32_t v) { h[o] = v & 255; h[o + 1] = (v >> 8) & 255; h[o + 2] = (v >> 16) & 255; h[o + 3] = (v >> 24) & 255; };
  w32(4, static_cast<uint32_t>(36 + n * 2));
  w32(24, rate);
  w32(28, rate * 2);
  h[32] = 2; h[33] = 0; h[34] = 16; h[35] = 0;
  memcpy(h + 36, "data", 4);
  w32(40, static_cast<uint32_t>(n * 2));
  fwrite(h, 1, 44, f);
  for (size_t i = 0; i < n; i++) {
    const unsigned char b[2] = {static_cast<unsigned char>(s[i] & 255), static_cast<unsigned char>((s[i] >> 8) & 255)};
    fwrite(b, 1, 2, f);
  }
  return fclose(f) == 0;
}

double rmsDb(const int16_t *s, size_t n) {
  if (!n) return -120.0;
  double sq = 0;
  for (size_t i = 0; i < n; i++) sq += double(s[i]) * s[i];
  return 10.0 * std::log10(sq / double(n) / (32768.0 * 32768.0) + 1e-12);
}
double peakDb(const int16_t *s, size_t n) {
  int peak = 0;
  for (size_t i = 0; i < n; i++) peak = std::max(peak, std::abs(int(s[i])));
  return 20.0 * std::log10(peak / 32768.0 + 1e-9);
}

}  // namespace

int main(int argc, char **argv) {
  const char *in = nullptr, *outdir = nullptr, *modi = nullptr;
  int loss = 0, floorDb = lcr::kDefaultMicFloorDb;
  bool raw = false;
  unsigned seed = 1;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--grezzo") raw = true;
    else if (a == "--modi" && i + 1 < argc) modi = argv[++i];
    else if (a == "--perdita" && i + 1 < argc) loss = std::atoi(argv[++i]);
    else if (a == "--fondo" && i + 1 < argc) floorDb = std::atoi(argv[++i]);
    else if (a == "--seme" && i + 1 < argc) seed = static_cast<unsigned>(std::atoi(argv[++i]));
    else if (!in) in = argv[i];
    else if (!outdir) outdir = argv[i];
    else { in = nullptr; break; }
  }
  if (!in || !outdir || loss < 0 || loss > 90 || floorDb < lcr::kMinMicFloorDb || floorDb > lcr::kMaxMicFloorDb) {
    std::fprintf(stderr,
                 "uso: codec_compare ingresso.wav cartella-uscita [--modi 3200,2400,1200,700C,450] [--perdita PERCENTO]\n"
                 "                   [--fondo DBFS] [--grezzo] [--seme N]\n"
                 "ingresso: WAV mono a 16 bit, 16000 Hz (come il microfono della scheda) o 8000 Hz\n"
                 "modi disponibili: 3200 2400 1600 1400 1300 1200 700C 450\n");
    return 2;
  }
  std::vector<int16_t> mic;
  uint32_t rate = 0;
  std::string err;
  if (!loadWav(in, mic, rate, err)) { std::fprintf(stderr, "%s: %s\n", in, err.c_str()); return 1; }

  // Si lavora a 16 kHz come il microfono della scheda: un ingresso a 8 kHz si porta a 16 kHz per interpolazione lineare.
  std::vector<int16_t> mic16;
  if (rate == 8000) {
    mic16.resize(mic.size() * 2);
    for (size_t i = 0; i < mic.size(); i++) {
      const int a = mic[i], b = (i + 1 < mic.size()) ? mic[i + 1] : mic[i];
      mic16[2 * i] = static_cast<int16_t>(a);
      mic16[2 * i + 1] = static_cast<int16_t>((a + b) / 2);
    }
  } else {
    mic16 = mic;
  }
  size_t frames40 = mic16.size() / lcr::kMicSamplesPerFrame;   // blocchi da 40 ms
  if (frames40 < 2) { std::fprintf(stderr, "registrazione troppo breve (meno di 80 ms)\n"); return 1; }
  mic16.resize(frames40 * lcr::kMicSamplesPerFrame);
  if (mkdir(outdir, 0777) && errno != EEXIST) { std::fprintf(stderr, "impossibile creare %s\n", outdir); return 1; }
  const std::string dir = outdir;

  // Catena audio della scheda (src/audio_io.cpp, frontConfig): passa-alto 300 Hz, decimazione, AGC a -24 dBFS.
  lcr::FrontEndConfig fc;
  fc.agc.maxGainDb = 30.0f;                              // LCR_MIC_AGC_MAX_GAIN_DB
  fc.agc.gateDbfs = lcr::micGateFromFloor(floorDb);
  fc.agc.idleGainDb = 12.0f;
  if (raw) {
    fc.agcEnabled = false;
    fc.fixedGainDb = 0.0f;
    fc.hpfHz = 20.0f;
  }
  lcr::AudioFrontEnd front(fc);
  std::vector<int16_t> pcm(frames40 * lcr::kFrameSamples);
  double gainSum = 0;
  for (size_t f = 0; f < frames40; f++) {
    int32_t block[lcr::kMicSamplesPerFrame];
    for (size_t i = 0; i < lcr::kMicSamplesPerFrame; i++) block[i] = static_cast<int32_t>(mic16[f * lcr::kMicSamplesPerFrame + i]) * 65536;
    front.processFrame(block, &pcm[f * lcr::kFrameSamples]);
    gainSum += front.gainDb();
  }

  saveWav(dir + "/originale.wav", mic16.data(), mic16.size(), 16000);
  saveWav(dir + "/ingresso-scheda.wav", pcm.data(), pcm.size(), 8000);
  const double secs = double(pcm.size()) / 8000.0;
  const double inRms = rmsDb(mic16.data(), mic16.size()), inPeak = peakDb(mic16.data(), mic16.size());
  const double outRms = rmsDb(pcm.data(), pcm.size());
  std::printf("Registrazione: %.1f s. Livello: RMS %.0f dBFS, picco %.0f dBFS.\n", secs, inRms, inPeak);
  std::printf("Dopo la catena audio della scheda%s: RMS %.0f dBFS (la scheda punta a -24), guadagno medio %.0f dB.\n",
              raw ? " (senza AGC, --grezzo)" : "", outRms, gainSum / double(frames40));
  if (inRms < -55.0) std::printf("ATTENZIONE: la registrazione e' molto bassa (sotto -55 dBFS): avvicinati al microfono o alza il volume di ingresso.\n");
  if (inPeak > -1.0) std::printf("ATTENZIONE: la registrazione satura (picco vicino a 0 dBFS): allontanati dal microfono o abbassa il volume di ingresso.\n");
  std::printf("Perdita simulata: %d%% dei frame.\n\n", loss);
  std::printf("%-5s %9s %7s %8s %12s %14s   %s\n", "modo", "bit/frame", "frame", "bit/s", "byte totali", "CPU PC (ms/s)", "file");

  std::vector<int16_t> dec(pcm.size());
  std::srand(seed);
  for (size_t k = 0; k < kNModes; k++) {
    if (modi) {
      const std::string list = std::string(",") + modi + ",", key = std::string(",") + kModes[k].name + ",";
      if (list.find(key) == std::string::npos) continue;
    }
    CODEC2 *c = codec2_create(kModes[k].mode);
    if (!c) { std::printf("%-5s non disponibile in questa libreria\n", kModes[k].name); continue; }
    const int spf = codec2_samples_per_frame(c), bpf = codec2_bits_per_frame(c), bytes = codec2_bytes_per_frame(c);
    std::vector<unsigned char> b(static_cast<size_t>(bytes) + 8, 0);
    const size_t frames = pcm.size() / static_cast<size_t>(spf);
    size_t lost = 0;
    const double t0 = nowS();
    for (size_t f = 0; f < frames; f++) {
      codec2_encode(c, b.data(), &pcm[f * static_cast<size_t>(spf)]);
      if (loss && (std::rand() % 100) < loss) { std::memset(b.data(), 0, static_cast<size_t>(bytes)); lost++; }
      codec2_decode(c, &dec[f * static_cast<size_t>(spf)], b.data());
    }
    const double cpu = nowS() - t0;
    const std::string path = dir + "/" + kModes[k].name + ".wav";
    if (!saveWav(path, dec.data(), frames * static_cast<size_t>(spf), 8000)) { std::fprintf(stderr, "scrittura di %s non riuscita\n", path.c_str()); return 1; }
    std::printf("%-5s %9d %7zu %8.0f %12.0f %14.1f   %s", kModes[k].name, bpf, frames, bpf * 8000.0 / spf, double(frames) * bpf / 8.0,
                cpu / secs * 1000.0, path.c_str());
    if (loss) std::printf("  (%zu frame persi)", lost);
    std::printf("\n");
    codec2_destroy(c);
  }
  std::printf("\nAscolta: ffplay -nodisp -autoexit %s/originale.wav ; %s/ingresso-scheda.wav (quello che il codec riceve) ; %s/1200.wav ; ...\n",
              dir.c_str(), dir.c_str(), dir.c_str());
  return 0;
}
