#include <cmath>
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

static void testSmoother() {
  BatterySmoother m;
  CHECK(m.update(4.00f) == 4.00f);  // la prima lettura parte da se stessa
  // Un gradino: tende al nuovo valore senza saltare.
  float v = 0;
  for (int i = 0; i < 20; i++) v = m.update(3.50f);
  CHECK(v > 3.50f && v < 4.00f);
  for (int i = 0; i < 600; i++) v = m.update(3.50f);  // 30 s
  CHECK(std::fabs(v - 3.50f) < 0.01f);

  // Caricatore che oscilla 4,14-4,42 V (T3 con solo USB): il valore mostrato resta in una banda stretta.
  BatterySmoother c;
  uint32_t seed = 4242;
  float lo = 9.0f, hi = 0.0f;
  for (int i = 0; i < 12000; i++) {  // 10 minuti
    seed = seed * 1664525u + 1013904223u;
    const float raw = 4.14f + 0.28f * static_cast<float>(seed >> 8) / 16777216.0f;
    const float shown = c.update(raw);
    if (i > 400) {  // dopo l'assestamento
      if (shown < lo) lo = shown;
      if (shown > hi) hi = shown;
    }
  }
  CHECK(hi - lo < 0.06f);          // contro 0,28 V del segnale grezzo
  CHECK(lo > 4.20f && hi < 4.35f);  // sempre mostrata, sempre un numero
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
  testSmoother();
  testFilter();
  TEST_MAIN_END();
}
