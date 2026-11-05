#include <cstring>

#include "battery_info.h"
#include "testing.h"

using namespace lcr;

static void testText() {
  char t[24];
  CHECK(formatBatteryText(t, sizeof(t), 4.12f) == 12 && strcmp(t, "(Batt=4.12V)") == 0);
  CHECK(strcmp((formatBatteryText(t, sizeof(t), 3.999f), t), "(Batt=4.00V)") == 0);
  CHECK(strcmp((formatBatteryText(t, sizeof(t), 0.0f), t), "(Batt=0.00V)") == 0);
  CHECK(strcmp((formatBatteryText(t, sizeof(t), 1.837f), t), "(Batt=1.84V)") == 0);
  // Non disponibile (nessuna batteria o nessun sensore).
  CHECK(formatBatteryText(t, sizeof(t), -1.0f) == 9 && strcmp(t, "(Batt=--)") == 0);
  // Il testo normale sta sempre in 12 caratteri = 72 pixel: non tocca "Vol 10/10" (54 pixel) a sinistra.
  for (float v = 0.0f; v < 10.0f; v += 0.37f) {
    CHECK(formatBatteryText(t, sizeof(t), v) <= 12);
  }
  // Buffer piccolo: niente scritture oltre il limite, stringa sempre terminata.
  char small[6];
  CHECK(formatBatteryText(small, sizeof(small), 4.12f) == 5 && small[5] == 0);
  CHECK(formatBatteryText(nullptr, 10, 4.0f) == 0 && formatBatteryText(t, 0, 4.0f) == 0);
}

static void testGuard() {
  BatteryGuard g;
  CHECK(g.filter(4.20f, 0) == 4.20f);
  CHECK(g.filter(4.35f, 1000) == 4.35f);
  CHECK(g.filter(-1.0f, 2000) == -1.0f);
  CHECK(g.filter(4.42f, 3000) == -1.0f);  // misurato sulla T3 con solo USB
  // Subito dopo, anche una lettura bassa resta "non disponibile".
  CHECK(g.filter(4.20f, 4000) == -1.0f);
  CHECK(g.filter(4.20f, 3000 + kBatteryHoldMs - 1) == -1.0f);
  CHECK(g.filter(4.20f, 3000 + kBatteryHoldMs) == 4.20f);
  CHECK(g.filter(0.0f, 3000 + kBatteryHoldMs + 10) == 0.0f);
  char t[24];
  BatteryGuard h;
  formatBatteryText(t, sizeof(t), h.filter(4.42f, 0));
  CHECK(strcmp(t, "(Batt=--)") == 0);
}

// Il caso che aveva dato un display instabile: il caricatore oscilla attorno al limite.
static void testNoFlapping() {
  BatteryGuard g;
  uint32_t seed = 4242;
  int transitions = 0;
  bool lastShown = true;
  bool first = true;
  for (uint32_t t = 0; t < 10 * 60 * 1000; t += 50) {  // 10 minuti, una lettura ogni 50 ms
    seed = seed * 1664525u + 1013904223u;
    const float v = 4.14f + 0.28f * static_cast<float>(seed >> 8) / 16777216.0f;  // 4,14...4,42 V come sulla T3
    const bool shown = g.filter(v, t) >= 0.0f;
    if (!first && shown != lastShown) transitions++;
    first = false;
    lastShown = shown;
  }
  CHECK(transitions <= 1);  // al massimo un passaggio da tensione a "--" all'avvio, poi stabile
  CHECK(!lastShown);

  // Una batteria vera (3,9 V con un po' di rumore) e' sempre mostrata.
  BatteryGuard b;
  bool always = true;
  for (uint32_t t = 0; t < 600000; t += 50) {
    seed = seed * 1664525u + 1013904223u;
    const float v = 3.90f + 0.04f * (static_cast<float>(seed >> 8) / 16777216.0f - 0.5f);
    always = always && b.filter(v, t) > 0.0f;
  }
  CHECK(always);
}

static void testLow() {
  CHECK(isLowBattery(3.39f));
  CHECK(isLowBattery(1.84f));
  CHECK(!isLowBattery(3.40f));
  CHECK(!isLowBattery(4.20f));
  CHECK(!isLowBattery(-1.0f));  // non disponibile: non e' "scarica"
}

static void testFilter() {
  AverageFilter f;
  CHECK(f.count() == 0 && f.average() == 0.0f);
  f.add(2.0f);
  f.add(4.0f);
  CHECK(f.count() == 2 && f.average() == 3.0f);
  // Finestra piena: restano solo le ultime kWindow letture.
  f.reset();
  for (size_t i = 0; i < AverageFilter::kWindow; i++) f.add(1.0f);
  CHECK(f.count() == AverageFilter::kWindow && f.average() == 1.0f);
  for (size_t i = 0; i < AverageFilter::kWindow; i++) f.add(3.0f);
  CHECK(f.count() == AverageFilter::kWindow && f.average() == 3.0f);
  // Un'unica lettura anomala pesa 1/kWindow.
  f.add(19.0f);
  CHECK(f.average() > 3.9f && f.average() < 4.2f);
  f.reset();
  CHECK(f.count() == 0 && f.average() == 0.0f);
}

int main() {
  testText();
  testLow();
  testGuard();
  testNoFlapping();
  testFilter();
  TEST_MAIN_END();
}
