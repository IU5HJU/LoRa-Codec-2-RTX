#include "position.h"

#include <cmath>
#include <cstdio>

namespace lcr {

bool isValidCoordinate(int32_t latE7, int32_t lonE7) {
  return latE7 >= -kMaxLatE7 && latE7 <= kMaxLatE7 && lonE7 >= -kMaxLonE7 && lonE7 <= kMaxLonE7;
}

namespace {
constexpr int64_t kSteps = int64_t{1} << 24;

uint32_t quantize(int32_t valueE7, int32_t maxE7) {
  // (v + max) / (2 max) * 2^24, arrotondato; 180 gradi esatti cadrebbero su 2^24: si limita all'ultimo passo.
  const int64_t num = (static_cast<int64_t>(valueE7) + maxE7) * kSteps;
  int64_t q = (num + maxE7) / (2 * static_cast<int64_t>(maxE7));
  if (q >= kSteps) q = kSteps - 1;
  if (q < 0) q = 0;
  return static_cast<uint32_t>(q);
}

int32_t dequantize(uint32_t q, int32_t maxE7) {
  return static_cast<int32_t>((static_cast<int64_t>(q) * 2 * maxE7 + kSteps / 2) / kSteps - maxE7);
}
}  // namespace

bool encodePosition(const Position& p, uint8_t* out) {
  if (!p.valid || !isValidCoordinate(p.latE7, p.lonE7)) return false;
  const uint32_t la = quantize(p.latE7, kMaxLatE7);
  const uint32_t lo = quantize(p.lonE7, kMaxLonE7);
  out[0] = p.fromGps ? 1 : 0;
  out[1] = static_cast<uint8_t>(la >> 16);
  out[2] = static_cast<uint8_t>(la >> 8);
  out[3] = static_cast<uint8_t>(la);
  out[4] = static_cast<uint8_t>(lo >> 16);
  out[5] = static_cast<uint8_t>(lo >> 8);
  out[6] = static_cast<uint8_t>(lo);
  return true;
}

void decodePosition(const uint8_t* in, Position& out) {
  const uint32_t la = (static_cast<uint32_t>(in[1]) << 16) | (static_cast<uint32_t>(in[2]) << 8) | in[3];
  const uint32_t lo = (static_cast<uint32_t>(in[4]) << 16) | (static_cast<uint32_t>(in[5]) << 8) | in[6];
  out.valid = true;
  out.fromGps = (in[0] & 1) != 0;
  out.latE7 = dequantize(la, kMaxLatE7);
  out.lonE7 = dequantize(lo, kMaxLonE7);
}

double distanceKm(const Position& a, const Position& b) {
  if (!a.valid || !b.valid) return 0.0;
  constexpr double kDeg = 3.14159265358979323846 / 180.0 * 1e-7;
  constexpr double kEarthKm = 6371.0;
  const double la1 = a.latE7 * kDeg, la2 = b.latE7 * kDeg;
  const double dla = la2 - la1;
  // La differenza in 64 bit (due longitudini opposte la farebbero uscire dai 32 bit); il seno al quadrato e' periodico, quindi
  // anche l'antimeridiano (179,5 E - 179,5 O) da il risultato giusto.
  const double dlo = static_cast<double>(static_cast<int64_t>(b.lonE7) - a.lonE7) * kDeg;
  const double s1 = std::sin(dla / 2), s2 = std::sin(dlo / 2);
  double h = s1 * s1 + std::cos(la1) * std::cos(la2) * s2 * s2;
  if (h > 1.0) h = 1.0;
  return 2.0 * kEarthKm * std::asin(std::sqrt(h));
}

size_t formatDistance(char* out, size_t cap, double km) {
  if (!out || cap == 0) return 0;
  int n;
  if (km < 0.1) n = snprintf(out, cap, "<0.1km");
  else if (km < 100.0) n = snprintf(out, cap, "%.1fkm", km);
  else n = snprintf(out, cap, "%.0fkm", km);
  return n < 0 ? 0 : (static_cast<size_t>(n) < cap ? static_cast<size_t>(n) : cap - 1);
}

}  // namespace lcr
