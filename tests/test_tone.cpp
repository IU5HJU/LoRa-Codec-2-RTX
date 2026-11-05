#include <cmath>
#include <cstdint>
#include <vector>

#include "testing.h"
#include "tone_meter.h"

using namespace lcr;

static std::vector<float> sine(float f, float amp, size_t n, float fs = 16000.0f, float phase = 0.3f) {
  std::vector<float> v(n);
  for (size_t i = 0; i < n; i++) v[i] = amp * std::sin(2.0f * 3.14159265f * f * static_cast<float>(i) / fs + phase);
  return v;
}

static void testAmplitude() {
  ToneMeter m;
  for (float f : {500.0f, 1000.0f, 2000.0f}) {
    for (float db : {-20.0f, -40.0f, -60.0f}) {
      m.begin(f, 16000.0f);
      auto x = sine(f, std::pow(10.0f, db / 20.0f), 640);
      m.addBlock(x.data(), x.size());
      CHECK(std::fabs(m.maxDb() - db) < 0.5f);  // il tono misurato e' l'ampiezza vera
    }
  }
}

static void testSelectivity() {
  // Un tono a un'altra frequenza, o rumore, non conta a 1000 Hz.
  ToneMeter m;
  m.begin(1000.0f, 16000.0f);
  auto other = sine(500.0f, 0.1f, 640);
  m.addBlock(other.data(), other.size());
  CHECK(m.maxDb() < -60.0f);
  m.begin(1000.0f, 16000.0f);
  uint32_t seed = 99;
  std::vector<float> noise(640);
  for (auto& v : noise) {
    seed = seed * 1664525u + 1013904223u;
    v = (static_cast<float>(seed >> 8) / 16777216.0f - 0.5f) * 0.1f;  // rumore forte (-26 dBFS)
  }
  m.addBlock(noise.data(), noise.size());
  CHECK(m.maxDb() < -45.0f);
  // Una componente continua (offset del microfono) non conta.
  m.begin(500.0f, 16000.0f);
  std::vector<float> dc(640, 0.3f);
  m.addBlock(dc.data(), dc.size());
  CHECK(m.maxDb() < -60.0f);
}

static void testMaxAndInteger() {
  ToneMeter m;
  CHECK(m.maxDb() == -120.0f && m.blocks() == 0);
  m.begin(1000.0f, 16000.0f);
  auto quiet = sine(1000.0f, 0.001f, 640);
  auto loud = sine(1000.0f, 0.1f, 640);
  m.addBlock(quiet.data(), 640);
  m.addBlock(loud.data(), 640);
  m.addBlock(quiet.data(), 640);
  CHECK(m.blocks() == 3 && std::fabs(m.maxDb() - (-20.0f)) < 0.5f);  // vale il blocco piu' forte
  // Campioni interi a 32 bit, come arrivano dall'I2S.
  m.begin(1000.0f, 16000.0f);
  std::vector<int32_t> s(640);
  for (size_t i = 0; i < s.size(); i++) s[i] = static_cast<int32_t>(2147483000.0 * 0.05 * std::sin(2.0 * 3.14159265 * 1000.0 * i / 16000.0));
  m.addBlock(s.data(), s.size());
  CHECK(std::fabs(m.maxDb() - (-26.0f)) < 0.7f);  // 0,05 = -26 dBFS
}

int main() {
  testAmplitude();
  testSelectivity();
  testMaxAndInteger();
  TEST_MAIN_END();
}
