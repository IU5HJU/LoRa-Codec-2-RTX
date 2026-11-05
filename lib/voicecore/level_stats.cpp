#include "level_stats.h"

#include <cmath>

namespace lcr {

void LevelStats::reset() { *this = LevelStats(); }

void LevelStats::add(float db) {
  int v = static_cast<int>(std::lround(db));
  if (v < kMinDb) v = kMinDb;
  if (v > kMaxDb) v = kMaxDb;
  bins_[v - kMinDb]++;
  count_++;
  if (v < min_) min_ = v;
  if (v > max_) max_ = v;
}

int LevelStats::percentile(int p) const {
  if (count_ == 0) return kMinDb;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  const uint32_t target = static_cast<uint32_t>((static_cast<uint64_t>(count_) * static_cast<uint32_t>(p) + 99) / 100);
  uint32_t acc = 0;
  for (int i = 0; i <= kMaxDb - kMinDb; i++) {
    acc += bins_[i];
    if (acc >= target && acc > 0) return kMinDb + i;
  }
  return max_;
}

}  // namespace lcr
