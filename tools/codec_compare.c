/* codec_compare: confronto dei modi del Codec2 sulla stessa registrazione.
 *
 * Usa gli STESSI sorgenti del Codec2 che gira sulle schede (third_party/esp32_codec2-1.0.7): quello che si sente qui e' quello
 * che si sentirebbe sulla scheda, a parte l'audio d'ingresso (qui un file, sulla scheda il microfono INMP441 con l'AGC).
 *
 *   make codec_compare
 *   arecord -f S16_LE -r 8000 -c 1 -d 10 voce.wav            (registra 10 s con il microfono del portatile)
 *   build/codec_compare voce.wav confronto                    (scrive confronto/*.wav e stampa la tabella)
 *   build/codec_compare voce.wav confronto --modi 3200,1200,700C,450 --perdita 5
 *
 * LIVELLO DEL MICROFONO. Il Codec2 e' molto sensibile al livello dell'ingresso (come FreeDV): sulle schede il filtro passa-alto
 * a 300 Hz e l'AGC portano la voce a circa -24 dBFS. Qui lo strumento misura la registrazione INTERA (parlato e rumore di
 * fondo), toglie il rimbombo con lo stesso passa-alto e applica UN guadagno che porta il livello del parlato a -24 dBFS:
 * funziona con un microfono basso, medio o alto (fino alla saturazione). Avvisa se la registrazione e' troppo bassa,
 * satura o ha troppo rumore di fondo. --livello misura soltanto e dice se il livello e' giusto; --grezzo salta la
 * normalizzazione (per sentire cosa succede senza).
 *
 * Ingresso: WAV PCM a 16 bit, mono, 8000 Hz (oppure s16le grezzo con --raw). Altri formati: 
 *   ffmpeg -i ingresso.wav -ar 8000 -ac 1 -sample_fmt s16 voce.wav
 * Uscita: originale.wav (la registrazione), ingresso-codec.wav (quello che il codec riceve, dopo passa-alto e guadagno) e
 * un file per ogni modo (3200.wav, 1200.wav, ...), tutti a 8 kHz.
 * --perdita N: butta via N% dei frame a caso (frame a zero, come fa la ricezione quando un pacchetto non si recupera).
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "codec2.h"

void *codec2_malloc(size_t n) { return malloc(n); }
void codec2_free(void *p) { free(p); }

static double now_s(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static const struct {
  const char *name;
  int mode;
} kModes[] = {
    {"3200", CODEC2_MODE_3200}, {"2400", CODEC2_MODE_2400}, {"1600", CODEC2_MODE_1600}, {"1400", CODEC2_MODE_1400},
    {"1300", CODEC2_MODE_1300}, {"1200", CODEC2_MODE_1200}, {"700C", CODEC2_MODE_700C}, {"450", CODEC2_MODE_450},
};
#define NMODES (sizeof(kModes) / sizeof(kModes[0]))

static uint32_t rd32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void wr32(unsigned char *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void wr16(unsigned char *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

/* Legge il file intero; ritorna i campioni (da liberare) o NULL con il motivo in *err. */
static short *load(const char *path, int raw, size_t *n, const char **err) {
  FILE *f = fopen(path, "rb");
  if (!f) { *err = "impossibile aprire il file"; return NULL; }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size <= 0 || size > 200L * 1000 * 1000) { fclose(f); *err = "file vuoto o troppo grande"; return NULL; }
  unsigned char *buf = malloc((size_t)size);
  if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(buf); *err = "lettura non riuscita"; return NULL; }
  fclose(f);
  const unsigned char *data = buf;
  size_t bytes = (size_t)size;
  if (!raw) {
    if (size < 44 || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) { free(buf); *err = "non e' un file WAV (usa --raw per s16le grezzo)"; return NULL; }
    size_t pos = 12;
    int fmt_ok = 0;
    data = NULL;
    while (pos + 8 <= (size_t)size) {
      uint32_t len = rd32(buf + pos + 4);
      if (!memcmp(buf + pos, "fmt ", 4) && pos + 8 + 16 <= (size_t)size) {
        uint16_t tag = rd16(buf + pos + 8), ch = rd16(buf + pos + 10), bits = rd16(buf + pos + 22);
        uint32_t rate = rd32(buf + pos + 12);
        if (tag != 1 || ch != 1 || bits != 16 || rate != 8000) {
          free(buf);
          *err = "serve un WAV PCM a 16 bit, mono, 8000 Hz (ffmpeg -i in.wav -ar 8000 -ac 1 -sample_fmt s16 voce.wav)";
          return NULL;
        }
        fmt_ok = 1;
      } else if (!memcmp(buf + pos, "data", 4)) {
        data = buf + pos + 8;
        bytes = len;
        /* un file interrotto (arecord chiuso male) puo' avere la lunghezza a zero o sbagliata: si usa tutto il resto del file */
        if (bytes == 0 || pos + 8 + bytes > (size_t)size) bytes = (size_t)size - pos - 8;
        break;
      }
      pos += 8 + len + (len & 1);
    }
    if (!fmt_ok || !data) { free(buf); *err = "WAV senza intestazione fmt o senza dati"; return NULL; }
  }
  *n = bytes / 2;
  short *s = malloc(*n * sizeof(short) + 2);
  if (!s) { free(buf); *err = "memoria esaurita"; return NULL; }
  for (size_t i = 0; i < *n; i++) s[i] = (short)(data[2 * i] | data[2 * i + 1] << 8);
  free(buf);
  return s;
}

static int save(const char *path, const short *s, size_t n) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  unsigned char h[44] = "RIFF\0\0\0\0WAVEfmt \x10\0\0\0\1\0\1\0\0\0\0\0\0\0\0\0\2\0\x10\0data\0\0\0\0";
  wr32(h + 4, (uint32_t)(36 + n * 2));
  wr32(h + 24, 8000);
  wr32(h + 28, 16000);
  wr16(h + 32, 2);
  wr32(h + 40, (uint32_t)(n * 2));
  fwrite(h, 1, 44, f);
  for (size_t i = 0; i < n; i++) {
    unsigned char b[2] = {(unsigned char)(s[i] & 255), (unsigned char)((s[i] >> 8) & 255)};
    fwrite(b, 1, 2, f);
  }
  return fclose(f);
}

/* ---- Livello: analisi e normalizzazione ---- */

#define kFrame 320 /* 40 ms a 8 kHz */

static int cmp_double(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}

/* Filtro passa-alto del 2o ordine (Butterworth) a 300 Hz, fs = 8000 Hz: gli stessi coefficienti della scheda
 * (lib/voicecore/audio_frontend.cpp, Biquad::setHighpass) con la frequenza di campionamento del codec. */
static void highpass300(const short *in, float *out, size_t n) {
  const double pi = 3.14159265358979, q = 0.70710678, w0 = 2.0 * pi * 300.0 / 8000.0;
  const double cw = cos(w0), alpha = sin(w0) / (2.0 * q), a0 = 1.0 + alpha;
  const double b0 = ((1.0 + cw) / 2.0) / a0, b1 = (-(1.0 + cw)) / a0, b2 = b0, a1 = (-2.0 * cw) / a0, a2 = (1.0 - alpha) / a0;
  double z1 = 0, z2 = 0;
  for (size_t i = 0; i < n; i++) {
    const double x = (double)in[i];
    const double y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    out[i] = (float)y;
  }
}

struct Level {
  double activeDb;   /* livello (RMS) del parlato, dBFS, dopo il passa-alto */
  double noiseDb;    /* livello del rumore di fondo (10 percentile dei frame), dBFS */
  double peakDb;     /* picco della registrazione, dBFS */
  double clipFrac;   /* frazione di campioni al limite (saturazione) */
};

/* Misura la registrazione dopo il passa-alto: parlato = i frame entro 15 dB dal 90 percentile. */
static int measure(const short *pcm, const float *hp, size_t n, struct Level *L) {
  size_t frames = n / kFrame;
  if (frames < 5) return -1;
  double *db = malloc(frames * sizeof(double));
  double *pw = malloc(frames * sizeof(double));
  if (!db || !pw) { free(db); free(pw); return -1; }
  for (size_t f = 0; f < frames; f++) {
    double sq = 0;
    for (size_t i = 0; i < kFrame; i++) { double v = hp[f * kFrame + i] / 32768.0; sq += v * v; }
    pw[f] = sq / kFrame;
    db[f] = 10.0 * log10(pw[f] + 1e-12);
  }
  double *sorted = malloc(frames * sizeof(double));
  memcpy(sorted, db, frames * sizeof(double));
  qsort(sorted, frames, sizeof(double), cmp_double);
  const double p90 = sorted[(frames - 1) * 9 / 10], p10 = sorted[(frames - 1) / 10];
  double sum = 0;
  size_t cnt = 0;
  for (size_t f = 0; f < frames; f++) {
    if (db[f] >= p90 - 15.0) { sum += pw[f]; cnt++; }
  }
  L->activeDb = cnt ? 10.0 * log10(sum / (double)cnt + 1e-12) : -120.0;
  L->noiseDb = p10;
  int peak = 0;
  size_t clipped = 0;
  for (size_t i = 0; i < n; i++) {
    int a = abs((int)pcm[i]);
    if (a > peak) peak = a;
    if (a >= 32700) clipped++;
  }
  L->peakDb = 20.0 * log10(peak / 32768.0 + 1e-9);
  L->clipFrac = (double)clipped / (double)n;
  free(db); free(pw); free(sorted);
  return 0;
}

/* Verdetto sul livello della registrazione grezza: SILENZIO, BASSO, ALTO o OK (con il motivo). */
static const char *verdict(const struct Level *L, const char **why) {
  if (L->peakDb < -70.0) { *why = "il microfono non registra niente: controlla che non sia disattivato (muto) o con il volume di ingresso a zero"; return "SILENZIO"; }
  if (L->clipFrac > 0.001 || L->peakDb > -0.5) { *why = "la registrazione satura (picchi al massimo): abbassa il volume di ingresso o allontanati dal microfono"; return "ALTO"; }
  if (L->activeDb < -50.0) { *why = "la voce e' molto bassa (sotto -50 dBFS): alza il volume di ingresso o avvicinati al microfono"; return "BASSO"; }
  if (L->activeDb - L->noiseDb < 15.0) { *why = "c'e' troppo rumore di fondo rispetto alla voce (meno di 15 dB di differenza): allontana il rumore o avvicinati al microfono"; return "RUMORE"; }
  *why = "livello giusto";
  return "OK";
}

int main(int argc, char **argv) {
  const char *in = NULL, *outdir = NULL, *modi = NULL;
  int raw = 0, loss = 0, onlyLevel = 0, nonorm = 0;
  double target = -24.0;
  unsigned seed = 1;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--raw")) raw = 1;
    else if (!strcmp(argv[i], "--modi") && i + 1 < argc) modi = argv[++i];
    else if (!strcmp(argv[i], "--perdita") && i + 1 < argc) loss = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--livello")) onlyLevel = 1;
    else if (!strcmp(argv[i], "--grezzo")) nonorm = 1;
    else if (!strcmp(argv[i], "--bersaglio") && i + 1 < argc) target = atof(argv[++i]);
    else if (!strcmp(argv[i], "--seme") && i + 1 < argc) seed = (unsigned)atoi(argv[++i]);
    else if (!in) in = argv[i];
    else if (!outdir) outdir = argv[i];
    else { in = NULL; break; }
  }
  if (!in || (!outdir && !onlyLevel) || loss < 0 || loss > 90 || target > -6.0 || target < -45.0) {
    fprintf(stderr,
            "uso: codec_compare ingresso.wav cartella-uscita [--modi 3200,2400,1200,700C,450] [--perdita PERCENTO] [--seme N] [--raw]\n"
            "                   [--grezzo] [--bersaglio DBFS]\n"
            "     codec_compare ingresso.wav --livello      (misura soltanto il livello e dice se e' giusto)\n"
            "modi disponibili: 3200 2400 1600 1400 1300 1200 700C 450\n");
    return 2;
  }
  size_t n = 0;
  const char *err = "";
  short *pcm = load(in, raw, &n, &err);
  if (!pcm) { fprintf(stderr, "%s: %s\n", in, err); return 1; }
  n -= n % 320;  /* multiplo di 40 ms: vale anche per i modi a 20 ms */
  if (n < 320) { fprintf(stderr, "registrazione troppo breve (meno di 40 ms)\n"); return 1; }

  /* Livello: misura sulla registrazione dopo il passa-alto a 300 Hz (come la scheda) */
  float *hp = malloc(n * sizeof(float));
  if (!hp) { fprintf(stderr, "memoria esaurita\n"); return 1; }
  highpass300(pcm, hp, n);
  struct Level L;
  if (measure(pcm, hp, n, &L)) { fprintf(stderr, "registrazione troppo breve per misurare il livello (servono almeno 0,2 secondi)\n"); return 1; }
  const char *why = "";
  const char *v = verdict(&L, &why);
  double secs = (double)n / 8000.0;
  printf("Registrazione: %.1f s. Livello del parlato %.0f dBFS, picco %.0f dBFS, rumore di fondo %.0f dBFS.\n", secs, L.activeDb, L.peakDb, L.noiseDb);
  printf("VERDETTO: %s - %s\n", v, why);
  if (onlyLevel) { free(hp); free(pcm); return 0; }
  if (!strcmp(v, "SILENZIO")) { fprintf(stderr, "Niente da confrontare: la registrazione e' silenziosa.\n"); return 1; }

  if (mkdir(outdir, 0777) && errno != EEXIST) { fprintf(stderr, "impossibile creare %s\n", outdir); return 1; }
  char path[512];
  snprintf(path, sizeof(path), "%s/originale.wav", outdir);
  if (save(path, pcm, n)) { fprintf(stderr, "scrittura di %s non riuscita\n", path); return 1; }

  /* Normalizzazione: passa-alto + UN guadagno che porta il parlato al bersaglio, senza far saturare i picchi. */
  short *codecIn = pcm;
  if (!nonorm) {
    double gainDb = target - L.activeDb;
    if (gainDb > 60.0) gainDb = 60.0;
    double peakAfter = L.peakDb + gainDb;   /* il picco dopo il passa-alto e' circa quello originale */
    if (peakAfter > -1.0) {                /* meglio un livello un po' piu' basso che saturare */
      double lowered = gainDb - (peakAfter + 1.0);
      gainDb = lowered < target - L.activeDb - 12.0 ? target - L.activeDb - 12.0 : lowered;
    }
    const double g = pow(10.0, gainDb / 20.0);
    codecIn = malloc(n * sizeof(short));
    for (size_t i = 0; i < n; i++) {
      double x = hp[i] * g;
      if (x > 32767.0) x = 32767.0;
      if (x < -32768.0) x = -32768.0;
      codecIn[i] = (short)lrint(x);
    }
    snprintf(path, sizeof(path), "%s/ingresso-codec.wav", outdir);
    save(path, codecIn, n);
    printf("Normalizzazione: guadagno %+.0f dB e passa-alto a 300 Hz: il codec riceve il parlato a circa %.0f dBFS (come la scheda).\n", gainDb, L.activeDb + gainDb);
  } else {
    printf("Senza normalizzazione (--grezzo): il codec riceve la registrazione cosi' com'e'.\n");
  }
  printf("Perdita simulata: %d%% dei frame.\n\n", loss);
  printf("%-5s %8s %8s %10s %12s %14s %16s\n", "modo", "bit/frame", "frame", "bit/s", "byte totali", "CPU PC (ms/s)", "file");

  short *dec = malloc(n * sizeof(short));
  srand(seed);
  for (size_t k = 0; k < NMODES; k++) {
    if (modi) {  /* elenco separato da virgole */
      char list[256];
      snprintf(list, sizeof(list), ",%s,", modi);
      char key[16];
      snprintf(key, sizeof(key), ",%s,", kModes[k].name);
      if (!strstr(list, key)) continue;
    }
    struct CODEC2 *c = codec2_create(kModes[k].mode);
    if (!c) { printf("%-5s non disponibile in questa libreria\n", kModes[k].name); continue; }
    int spf = codec2_samples_per_frame(c), bpf = codec2_bits_per_frame(c), bytes = codec2_bytes_per_frame(c);
    unsigned char *b = calloc((size_t)bytes + 8, 1);
    size_t frames = n / (size_t)spf, lost = 0;
    double t0 = now_s();
    for (size_t f = 0; f < frames; f++) {
      codec2_encode(c, b, codecIn + f * (size_t)spf);
      if (loss && (rand() % 100) < loss) { memset(b, 0, (size_t)bytes); lost++; }
      codec2_decode(c, dec + f * (size_t)spf, b);
    }
    double cpu = now_s() - t0;
    snprintf(path, sizeof(path), "%s/%s.wav", outdir, kModes[k].name);
    if (save(path, dec, frames * (size_t)spf)) { fprintf(stderr, "scrittura di %s non riuscita\n", path); return 1; }
    printf("%-5s %8d %8zu %10.0f %12.0f %14.1f   %s", kModes[k].name, bpf, frames, bpf * 8000.0 / spf,
           (double)frames * bpf / 8.0, cpu / secs * 1000.0, path);
    if (loss) printf("  (%zu frame persi)", lost);
    printf("\n");
    codec2_destroy(c);
    free(b);
  }
  printf("\nAscolta e confronta: ffplay -nodisp -autoexit %s/originale.wav ; %s/ingresso-codec.wav (quello che il codec riceve) ; %s/1200.wav ; ...\n", outdir, outdir, outdir);
  free(dec);
  if (codecIn != pcm) free(codecIn);
  free(hp);
  free(pcm);
  return 0;
}
