#include <cmath>
#include <vector>

#include "audio_backend.h"
#include "testing.h"

using namespace lcr;

static const double kPi = 3.14159265358979;

static double dbToLin(double db) { return std::pow(10.0, db / 20.0); }
static double linToDb(double x) { return 20.0 * std::log10(x + 1e-12); }

static BackEndConfig plain() {
  BackEndConfig c;
  c.agcEnabled = false;
  c.volumeDb = 0.0f;
  return c;
}

struct Result {
  double rmsDb;
  int peak;
  std::vector<int16_t> samples;  // ultimo frame
};

// Elabora un seno (ampiezza di picco ampDb dBFS, a 8 kHz) e misura l'ultimo frame.
static Result runTone(AudioBackEnd& be, double freq, double ampDb, int nFrames) {
  Result r{};
  size_t n = 0;
  const double amp = dbToLin(ampDb);
  for (int f = 0; f < nFrames; f++) {
    int16_t in[kFrameSamples];
    for (size_t i = 0; i < kFrameSamples; i++, n++) {
      in[i] = static_cast<int16_t>(std::lround(32767.0 * amp * std::sin(2.0 * kPi * freq * static_cast<double>(n) / 8000.0)));
    }
    std::vector<int16_t> out(be.outputSamplesPerFrame());
    be.processFrame(in, out.data());
    if (f == nFrames - 1) {
      double sumSq = 0;
      r.peak = 0;
      for (int16_t v : out) {
        sumSq += (v / 32768.0) * (v / 32768.0);
        if (std::abs(v) > r.peak) r.peak = std::abs(v);
      }
      r.rmsDb = linToDb(std::sqrt(sumSq / static_cast<double>(out.size())));
      r.samples = out;
    }
  }
  return r;
}

// Energia (dB) a una frequenza, con Goertzel.
static double goertzelDb(const std::vector<int16_t>& x, double freq, double fs) {
  const double w = 2.0 * kPi * freq / fs;
  const double c = 2.0 * std::cos(w);
  double s1 = 0, s2 = 0;
  for (int16_t v : x) {
    const double s = v / 32768.0 + c * s1 - s2;
    s2 = s1;
    s1 = s;
  }
  const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
  return 10.0 * std::log10(p / static_cast<double>(x.size()) / static_cast<double>(x.size()) + 1e-20);
}

static void testPassthrough() {
  AudioBackEnd be(plain());
  const Result r = runTone(be, 1000, -30, 4);
  CHECK(std::fabs(r.rmsDb - (-30 - 3.01)) < 0.3);
}

static void testVolume() {
  AudioBackEnd be(plain());
  const double ref = runTone(be, 1000, -30, 4).rmsDb;
  be.setVolumeDb(-6);
  const double half = runTone(be, 1000, -30, 4).rmsDb;
  CHECK(std::fabs((ref - half) - 6.0) < 0.3);

  be.setVolumeStep(5);  // il passo 5 e' 0 dB (il vecchio massimo)
  CHECK(std::fabs(be.volumeDb()) < 0.01 && !be.muted());
  be.setVolumeStep(10);  // guadagno: +15 dB
  CHECK(std::fabs(be.volumeDb() - 15.0) < 0.01);
  be.setVolumeStep(7);
  CHECK(std::fabs(be.volumeDb() - 6.0) < 0.01);
  be.setVolumeStep(1);
  CHECK(std::fabs(be.volumeDb() - (-12.0)) < 0.01);
  be.setVolumeStep(0);
  CHECK(be.muted());
  const Result silent = runTone(be, 1000, -10, 4);
  CHECK(silent.peak == 0);
  be.setVolumeStep(3);  // il muto si toglie
  CHECK(!be.muted() && runTone(be, 1000, -10, 4).peak > 0);

  be.setVolumeDb(+30);  // mai oltre +15 dB
  CHECK(std::fabs(be.volumeDb() - 15.0) < 0.01);
  be.setVolumeDb(-200);
  CHECK(be.volumeDb() >= -60.0f);
}

static void testLimiter() {
  // Sotto soglia (0,6 = -4,4 dBFS) il segnale non e' toccato.
  AudioBackEnd a(plain());
  const Result low = runTone(a, 700, -8, 4);
  CHECK(std::fabs(low.rmsDb - (-8 - 3.01)) < 0.3);

  // Fondo scala con guadagno: il picco resta sotto maxPeak (0,9), senza tagli duri.
  BackEndConfig c = plain();
  c.agcEnabled = false;
  AudioBackEnd b(c);
  std::vector<int16_t> out(kFrameSamples);
  int16_t in[kFrameSamples];
  int peak = 0;
  for (int f = 0; f < 4; f++) {
    for (size_t i = 0; i < kFrameSamples; i++) in[i] = (i / 8) % 2 ? 32767 : -32768;
    b.processFrame(in, out.data());
    for (int16_t v : out) peak = std::max(peak, static_cast<int>(std::abs(v)));
  }
  CHECK(peak <= static_cast<int>(0.9 * 32768.0) + 1);
  CHECK(peak > static_cast<int>(0.85 * 32768.0));  // ma ci si avvicina
}

static void testAgcLevelsStations() {
  // Stazioni con livelli molto diversi arrivano allo stesso livello in uscita.
  BackEndConfig c;
  c.volumeDb = 0.0f;
  AudioBackEnd quiet(c), loud(c);
  const double q = runTone(quiet, 800, -45, 80).rmsDb;
  const double l = runTone(loud, 800, -12, 80).rmsDb;
  CHECK(std::fabs(q - (-24.0)) < 3.0);
  CHECK(std::fabs(l - (-24.0)) < 3.0);
}

static void testLevelReadout() {
  // Senza AGC e a 0 dB il livello in uscita e' quello in ingresso; un seno di picco -20 dBFS ha RMS -23 dBFS.
  AudioBackEnd be(plain());
  runTone(be, 440.0, -20.0, 20);
  CHECK(std::fabs(be.inputDb() - (-23.0)) < 0.6);
  CHECK(std::fabs(be.outputDb() - (-23.0)) < 0.8);
  CHECK(!be.limited());
  // Con il volume a -12 dB l'uscita scende di 12 dB, l'ingresso resta.
  BackEndConfig c = plain();
  c.volumeDb = -12.0f;
  AudioBackEnd quiet(c);
  runTone(quiet, 440.0, -20.0, 20);
  CHECK(std::fabs(quiet.inputDb() - (-23.0)) < 0.6);
  CHECK(std::fabs(quiet.outputDb() - (-35.0)) < 0.8);
  // Segnale forte: interviene il limitatore e l'uscita e' sotto l'ingresso.
  AudioBackEnd loud(plain());
  runTone(loud, 440.0, -1.0, 20);
  CHECK(loud.limited() && loud.outputDb() < loud.inputDb());
  // Con l'AGC il guadagno e' leggibile e porta il parlato debole verso il target di -24 dBFS.
  BackEndConfig a;
  a.volumeDb = 0.0f;
  AudioBackEnd agc(a);
  runTone(agc, 440.0, -45.0, 80);
  CHECK(agc.agcGainDb() > 10.0f && std::fabs(agc.outputDb() - (-24.0)) < 4.0);
}

static void testUpsample() {
  BackEndConfig c = plain();
  c.upsample2x = true;
  AudioBackEnd be(c);
  CHECK(be.outputSamplesPerFrame() == 2 * kFrameSamples);

  const Result tone = runTone(be, 1000, -20, 6);
  CHECK(tone.samples.size() == 640);
  CHECK(std::fabs(tone.rmsDb - (-20 - 3.01)) < 1.0);

  // 3 kHz a 8 kHz: l'immagine a 5 kHz (16 - ... = 8 - 3) deve essere molto attenuata.
  AudioBackEnd be2(c);
  const Result t3 = runTone(be2, 3000, -20, 6);
  const double main = goertzelDb(t3.samples, 3000, 16000);
  const double image = goertzelDb(t3.samples, 5000, 16000);
  CHECK(main - image > 25.0);
}

static void testLostFrameIsSilence() {
  BackEndConfig c = plain();
  AudioBackEnd be(c);
  int16_t out[kFrameSamples];
  for (auto& v : out) v = 123;
  be.processLost(out);
  bool allZero = true;
  for (auto v : out) allZero = allZero && v == 0;
  CHECK(allZero);
  // E il frame successivo e' normale.
  CHECK(runTone(be, 1000, -20, 3).peak > 0);

  BackEndConfig u = plain();
  u.upsample2x = true;
  AudioBackEnd bu(u);
  int16_t out2[2 * kFrameSamples];
  for (auto& v : out2) v = 123;
  bu.processLost(out2);
  allZero = true;
  for (auto v : out2) allZero = allZero && v == 0;
  CHECK(allZero);
}

int main() {
  testPassthrough();
  testVolume();
  testLimiter();
  testAgcLevelsStations();
  testLevelReadout();
  testUpsample();
  testLostFrameIsSilence();
  TEST_MAIN_END();
}
