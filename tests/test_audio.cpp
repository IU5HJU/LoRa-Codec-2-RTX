#include <cmath>
#include <vector>

#include "audio_frontend.h"
#include "preroll_mic.h"
#include "testing.h"

using namespace lcr;

static const double kPi = 3.14159265358979;

// Parola I2S a 32 bit: campione a 24 bit allineato a sinistra.
static int32_t i2sWord(double v) {
  if (v > 0.9999) v = 0.9999;
  if (v < -0.9999) v = -0.9999;
  return static_cast<int32_t>(std::lround(v * 8388607.0)) * 256;
}

static double dbToLin(double db) { return std::pow(10.0, db / 20.0); }
static double linToDb(double x) { return 20.0 * std::log10(x + 1e-12); }

// Genera nFrames frame di un seno (ampiezza di picco ampDb dBFS) e li elabora.
// Ritorna l'RMS (dBFS) dell'uscita sugli ultimi 'tail' frame.
static double runTone(AudioFrontEnd& fe, double freq, double ampDb, int nFrames, int tail, size_t* peak = nullptr) {
  const double amp = dbToLin(ampDb);
  size_t n = 0;
  double sumSq = 0;
  size_t cnt = 0;
  if (peak) *peak = 0;
  for (int f = 0; f < nFrames; f++) {
    int32_t in[kMicSamplesPerFrame];
    for (size_t i = 0; i < kMicSamplesPerFrame; i++, n++) {
      in[i] = i2sWord(amp * std::sin(2.0 * kPi * freq * static_cast<double>(n) / 16000.0));
    }
    int16_t out[kFrameSamples];
    fe.processFrame(in, out);
    if (f >= nFrames - tail) {
      for (size_t i = 0; i < kFrameSamples; i++) {
        const double v = out[i] / 32768.0;
        sumSq += v * v;
        cnt++;
        const size_t a = static_cast<size_t>(std::abs(static_cast<int>(out[i])));
        if (peak && a > *peak) *peak = a;
      }
    }
  }
  return linToDb(std::sqrt(sumSq / static_cast<double>(cnt)));
}

static FrontEndConfig fixedGain(float gainDb) {
  FrontEndConfig c;
  c.agcEnabled = false;
  c.fixedGainDb = gainDb;
  return c;
}

static void testPassband() {
  // Con AGC spento e guadagno 0 dB, 1 kHz passa con guadagno ~1.
  AudioFrontEnd fe(fixedGain(0));
  const double rms = runTone(fe, 1000, -10, 30, 10);
  const double expected = -10 - 3.01;  // RMS di un seno
  CHECK(std::fabs(rms - expected) < 1.0);
}

static void testHighpass() {
  // Sotto i 300 Hz attenuazione forte; a 100 Hz oltre 15 dB.
  AudioFrontEnd a(fixedGain(0)), b(fixedGain(0));
  const double ref = runTone(a, 1000, -10, 40, 10);
  const double low = runTone(b, 100, -10, 40, 10);
  CHECK(ref - low > 15.0);
}

static void testDcRemoved() {
  AudioFrontEnd fe(fixedGain(0));
  int16_t out[kFrameSamples];
  for (int f = 0; f < 60; f++) {
    int32_t in[kMicSamplesPerFrame];
    for (size_t i = 0; i < kMicSamplesPerFrame; i++) in[i] = i2sWord(0.2);  // offset in continua
    fe.processFrame(in, out);
  }
  double sum = 0;
  for (size_t i = 0; i < kFrameSamples; i++) sum += std::abs(out[i]);
  CHECK(sum / kFrameSamples < 20.0);  // meno di ~-64 dBFS
}

static void testAntiAliasing() {
  // 6 kHz a 16 kHz si ripiegherebbe a 2 kHz senza filtro: deve essere molto attenuato.
  AudioFrontEnd a(fixedGain(0)), b(fixedGain(0));
  const double ref = runTone(a, 1000, -10, 30, 10);
  const double alias = runTone(b, 6000, -10, 30, 10);
  CHECK(ref - alias > 30.0);
}

static void testAgcRaisesQuietSignal() {
  AudioFrontEnd fe;  // AGC attivo, target -24 dBFS RMS
  // Voce a distanza: RMS ~ -60 dBFS (picco -57).
  const double rms = runTone(fe, 800, -57, 80, 10);
  CHECK(std::fabs(rms - (-24.0)) < 3.0);
  CHECK(fe.gainDb() > 30.0 && fe.gainDb() <= 36.01);
}

static void testAgcLowersLoudSignal() {
  AudioFrontEnd fe;
  size_t peak = 0;
  const double rms = runTone(fe, 800, -2, 60, 10, &peak);  // quasi fondo scala
  CHECK(std::fabs(rms - (-24.0)) < 3.0);
  CHECK(peak < 32768);
  CHECK(fe.gainDb() < 0.0);
}

static void testAgcIgnoresNoise() {
  // Rumore sotto la soglia: il guadagno non sale (non si amplifica il fondo).
  AudioFrontEnd fe;
  const float g0 = fe.gainDb();
  uint32_t seed = 12345;
  for (int f = 0; f < 100; f++) {
    int32_t in[kMicSamplesPerFrame];
    for (size_t i = 0; i < kMicSamplesPerFrame; i++) {
      seed = seed * 1664525u + 1013904223u;
      const double n = (static_cast<double>(seed >> 8) / 16777216.0 - 0.5) * dbToLin(-80);
      in[i] = i2sWord(n);
    }
    int16_t out[kFrameSamples];
    fe.processFrame(in, out);
  }
  CHECK(std::fabs(fe.gainDb() - g0) < 0.1);
}

static void testClipsWithoutWrapping() {
  // Guadagno enorme su un'onda quadra forte: l'uscita satura ai limiti.
  AudioFrontEnd fe(fixedGain(36));
  int16_t out[kFrameSamples];
  int maxV = -32768, minV = 32767, atRail = 0, total = 0;
  size_t n = 0;
  for (int f = 0; f < 10; f++) {
    int32_t in[kMicSamplesPerFrame];
    for (size_t i = 0; i < kMicSamplesPerFrame; i++, n++) in[i] = i2sWord(((n / 16) % 2) ? 0.5 : -0.5);
    fe.processFrame(in, out);
    if (f < 3) continue;
    for (size_t i = 0; i < kFrameSamples; i++) {
      if (out[i] > maxV) maxV = out[i];
      if (out[i] < minV) minV = out[i];
      total++;
      if (out[i] == 32767 || out[i] == -32768) atRail++;
    }
  }
  CHECK(maxV == 32767 && minV == -32768);
  CHECK(atRail > total / 2);  // onda quadra: la maggior parte dei campioni e' ai limiti
}

static void testPrerollMic() {
  PrerollMic mic(3);
  int16_t pcm[kFrameSamples];
  int16_t rd[kFrameSamples];
  auto push = [&](int id) {
    for (size_t i = 0; i < kFrameSamples; i++) pcm[i] = static_cast<int16_t>(id);
    mic.feed(pcm);
  };
  // Microfono sempre acceso, PTT rilasciato: si tengono solo gli ultimi 3 frame.
  for (int i = 1; i <= 10; i++) push(i);
  CHECK(mic.buffered() == 3);
  CHECK(!mic.readFrame(rd));  // non in registrazione

  mic.start();
  // Il pre-roll (frame 8, 9, 10) e' disponibile subito, poi l'audio in diretta.
  push(11);
  int got[8];
  int n = 0;
  while (n < 8 && mic.readFrame(rd)) got[n++] = rd[0];
  CHECK(n == 4);
  CHECK(got[0] == 8 && got[1] == 9 && got[2] == 10 && got[3] == 11);

  push(12);
  CHECK(mic.readFrame(rd) && rd[0] == 12);

  mic.stop();
  CHECK(mic.buffered() == 0 && !mic.readFrame(rd));
  push(13);
  push(14);
  CHECK(mic.buffered() == 2);  // di nuovo solo pre-roll
  CHECK(mic.overruns() == 0);
}

static void testPrerollOverrun() {
  PrerollMic mic(2);
  int16_t pcm[kFrameSamples] = {0};
  mic.start();
  for (size_t i = 0; i < PrerollMic::kCapacity + 3; i++) mic.feed(pcm);
  CHECK(mic.overruns() == 3);
  CHECK(mic.buffered() == PrerollMic::kCapacity);
}

int main() {
  testPassband();
  testHighpass();
  testDcRemoved();
  testAntiAliasing();
  testAgcRaisesQuietSignal();
  testAgcLowersLoudSignal();
  testAgcIgnoresNoise();
  testClipsWithoutWrapping();
  testPrerollMic();
  testPrerollOverrun();
  TEST_MAIN_END();
}
