#include "battery_info.h"

#include <cstdio>

namespace lcr {

size_t formatBatteryText(char* out, size_t cap, float volts) {
  if (!out || cap == 0) return 0;
  int n;
  if (volts < 0.0f) {
    n = std::snprintf(out, cap, "(Batt=--)");
  } else {
    n = std::snprintf(out, cap, "(Batt=%.2fV)", static_cast<double>(volts));
  }
  if (n < 0) {
    out[0] = 0;
    return 0;
  }
  return static_cast<size_t>(n) < cap ? static_cast<size_t>(n) : cap - 1;
}

bool isLowBattery(float volts) { return volts >= 0.0f && volts < kLowBatteryVolts; }

float BatteryGuard::filter(float volts, uint32_t nowMs) {
  if (volts < 0.0f) return -1.0f;
  if (volts > kMaxBatteryVolts) {
    seen_ = true;
    lastBadMs_ = nowMs;
  }
  if (seen_ && nowMs - lastBadMs_ < kBatteryHoldMs) return -1.0f;
  return volts;
}

}  // namespace lcr
