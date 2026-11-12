// Prove radio del Morse: lettura del comando e piano dei pacchetti.
#include <cmath>
#include <cstring>

#include "morse.h"
#include "radio_test.h"
#include "testing.h"

using namespace lcr;

static bool parse(const char* a, RadioTestRequest& r, char* err) {
  err[0] = 0;
  return parseRadioTest(a, r, err, 100);
}

static void testParse() {
  RadioTestRequest r;
  char e[100];
  CHECK(parse("MORSE FM IR5XXX", r, e) && r.kind == RadioTestKind::Morse && r.fm && strcmp(r.text, "IR5XXX") == 0 && r.wpm == 15 && r.toneHz == 700 && r.devKhz == 2.5f && r.ramp == -1 && r.shape == 0);
  CHECK(parse("morse cw iz0rpt wpm=20", r, e) && !r.fm && strcmp(r.text, "IZ0RPT") == 0 && r.wpm == 20);
  CHECK(parse("MORSE FM IR5_XXX hz=800 dev=3 ramp=0 shape=0.5", r, e) && strcmp(r.text, "IR5 XXX") == 0 && r.toneHz == 800 && r.devKhz == 3 && r.ramp == 0 && r.shape == 5);
  CHECK(parse("BEACON FM IR5XXX 600 wpm=12", r, e) && r.kind == RadioTestKind::Beacon && r.periodS == 600 && r.wpm == 12);
  CHECK(parse("CARRIER", r, e) && r.kind == RadioTestKind::Carrier && !r.fm && r.seconds == 5);
  CHECK(parse("CARRIER 10 ramp=3", r, e) && r.seconds == 10 && r.ramp == 3);
  CHECK(parse("TONE 8 hz=1000 dev=2.5", r, e) && r.kind == RadioTestKind::Tone && r.fm && r.seconds == 8 && r.toneHz == 1000);
  CHECK(parse("TONE", r, e) && r.seconds == 5);
  // errori
  CHECK(!parse("", r, e) && e[0]);
  CHECK(!parse("MORSE", r, e));
  CHECK(!parse("MORSE SSB IR5XXX", r, e) && strstr(e, "FM oppure CW"));
  CHECK(!parse("MORSE FM IR5XXX wpm=3", r, e) && strstr(e, "wpm"));
  CHECK(!parse("MORSE FM IR5XXX wpm=31", r, e));
  CHECK(!parse("MORSE FM IR5XXX hz=100", r, e));
  CHECK(!parse("MORSE FM IR5XXX dev=0.1", r, e));
  CHECK(!parse("MORSE FM IR5XXX dev=11", r, e));
  CHECK(!parse("MORSE FM IR5XXX ramp=16", r, e));
  CHECK(!parse("MORSE FM IR5XXX shape=0.7", r, e));
  CHECK(!parse("MORSE FM IR5XXX colore=rosso", r, e));
  CHECK(!parse("MORSE FM IR5/XXX", r, e) && strstr(e, "solo lettere"));
  CHECK(!parse("MORSE FM ABCDEFGHIJKLMNOPQ", r, e));  // 17 caratteri
  CHECK(!parse("BEACON FM IR5XXX", r, e) && strstr(e, "periodo"));
  CHECK(!parse("BEACON FM IR5XXX 5", r, e));
  CHECK(!parse("CARRIER 31", r, e));
  CHECK(!parse("CARRIER 0", r, e));
  CHECK(!parse("QRZ", r, e));
}

static void testBitRate() {
  RadioTestRequest r;
  char e[100];
  CHECK(parse("MORSE FM IR5XXX hz=700", r, e) && std::fabs(radioTestBitRateKbps(r) - 1.4f) < 1e-4);
  CHECK(radioTestPattern(r) == 0xAA);
  CHECK(parse("MORSE CW IR5XXX", r, e) && radioTestBitRateKbps(r) == kCwBitRateKbps && radioTestPattern(r) == 0xFF);
  CHECK(parse("CARRIER", r, e) && radioTestPattern(r) == 0xFF && radioTestBitRateKbps(r) == kCwBitRateKbps);
  CHECK(parse("TONE hz=1000", r, e) && radioTestPattern(r) == 0xAA && std::fabs(radioTestBitRateKbps(r) - 2.0f) < 1e-4);
  CHECK(parse("BEACON FM IR5XXX 60 hz=500", r, e) && std::fabs(radioTestBitRateKbps(r) - 1.0f) < 1e-4);
  CHECK(std::fabs(burstAirMs(14, 1.4f) - 80.0f) < 0.01f);
}

static void testPlan() {
  RadioTestRequest r;
  char e[100];
  MorseBurst b[160];
  bool clamped = true;
  // "E" = un punto; a 15 wpm (unita' 80 ms) in FM a 700 Hz: 14 byte
  CHECK(parse("MORSE FM E", r, e));
  CHECK(planMorseBursts(r, b, 160, &clamped) == 1 && !clamped && b[0].tone && b[0].ms == 80 && b[0].bytes == 14);
  // "T" = una linea: 240 ms = 42 byte
  CHECK(parse("MORSE FM T", r, e) && planMorseBursts(r, b, 160, nullptr) == 1 && b[0].bytes == 42);
  // CW: 80 ms a 2400 bit/s = 24 byte
  CHECK(parse("MORSE CW E", r, e) && planMorseBursts(r, b, 160, nullptr) == 1 && b[0].bytes == 24);
  // "IR5XXX" in FM a 15 wpm: la durata dei pacchetti e delle pause coincide con quella del Morse (entro un byte per elemento)
  CHECK(parse("MORSE FM IR5XXX", r, e));
  const size_t n = planMorseBursts(r, b, 160, &clamped);
  MorseElement el[160];
  CHECK(!clamped && n == morseElements("IR5XXX", 80, el, 160));
  double air = 0, want = 0;
  for (size_t i = 0; i < n; i++) {
    CHECK(b[i].tone == el[i].tone && b[i].ms == el[i].ms);
    want += el[i].ms;
    air += b[i].tone ? burstAirMs(b[i].bytes, radioTestBitRateKbps(r)) : b[i].ms;
    if (b[i].tone) CHECK(std::fabs(burstAirMs(b[i].bytes, radioTestBitRateKbps(r)) - b[i].ms) <= 8.0 / 1.4 / 2 + 0.01);  // mezzo byte
  }
  CHECK(std::fabs(air - want) / want < 0.02);
  CHECK(want > 5000 && want < 9000);  // un nominativo di 6 caratteri: circa 6-7 s a 15 wpm
  // piu' lento: 5 wpm, la linea dura 720 ms = 126 byte a 1,4 kbit/s
  CHECK(parse("MORSE FM T wpm=5", r, e) && planMorseBursts(r, b, 160, nullptr) == 1 && b[0].ms == 720 && b[0].bytes == 126);
  // un tono troppo acuto per un pacchetto solo: accorciato e segnalato
  CHECK(parse("MORSE FM T wpm=5 hz=2500", r, e));
  CHECK(planMorseBursts(r, b, 160, &clamped) == 1 && clamped && b[0].bytes == 255);
  // spazio tra parole e capienza dell'elenco
  CHECK(parse("MORSE CW E_E", r, e));
  CHECK(planMorseBursts(r, b, 160, nullptr) == 3 && !b[1].tone && b[1].ms == 7 * 80);
  CHECK(planMorseBursts(r, b, 2, nullptr) == 2);
}

int main() {
  testParse();
  testBitRate();
  testPlan();
  TEST_MAIN_END();
}
