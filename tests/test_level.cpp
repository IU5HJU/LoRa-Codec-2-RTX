#include "level_stats.h"
#include "testing.h"

using namespace lcr;

static void testBasic() {
  LevelStats s;
  CHECK(s.count() == 0 && s.percentile(90) == LevelStats::kMinDb);
  for (int i = 0; i < 100; i++) s.add(-60.0f);
  CHECK(s.count() == 100 && s.minDb() == -60 && s.maxDb() == -60);
  CHECK(s.percentile(0) == -60 && s.percentile(90) == -60 && s.percentile(100) == -60);
}

static void testSpikeIgnored() {
  // Un colpo isolato durante la misura non sposta il 90% (ma si vede nel massimo).
  LevelStats s;
  for (int i = 0; i < 74; i++) s.add(-61.0f + static_cast<float>(i % 3));  // -61...-59
  s.add(-25.0f);  // un colpo
  CHECK(s.count() == 75);
  CHECK(s.maxDb() == -25 && s.minDb() == -61);
  CHECK(s.percentile(90) <= -59);
  CHECK(s.percentile(100) == -25);
}

static void testClampAndRounding() {
  LevelStats s;
  s.add(-200.0f);
  s.add(5.0f);
  s.add(-60.4f);
  s.add(-60.6f);
  CHECK(s.minDb() == LevelStats::kMinDb && s.maxDb() == 0);
  s.reset();
  CHECK(s.count() == 0);
  s.add(-60.4f);
  s.add(-60.6f);
  CHECK(s.minDb() == -61 && s.maxDb() == -60);
}

static void testPercentileOrder() {
  LevelStats s;
  for (int i = 1; i <= 100; i++) s.add(-100.0f + static_cast<float>(i) / 2.0f);  // da -99,5 a -50
  CHECK(s.percentile(50) <= s.percentile(90));
  CHECK(s.percentile(90) <= s.percentile(100));
  CHECK(s.percentile(90) >= -56 && s.percentile(90) <= -54);
}

int main() {
  testBasic();
  testSpikeIgnored();
  testClampAndRounding();
  testPercentileOrder();
  TEST_MAIN_END();
}
