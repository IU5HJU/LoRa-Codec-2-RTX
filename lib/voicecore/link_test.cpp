#include "link_test.h"

#include <cmath>
#include <cstring>

namespace lcr {

void testFramePattern(uint16_t session, uint32_t index, uint8_t* out) {
  // xorshift32 inizializzato da sessione e indice: i frame cambiano ad ogni prova e ad ogni posizione.
  uint32_t x = (static_cast<uint32_t>(session) << 16) ^ (index * 2654435761u) ^ 0x9E3779B9u;
  if (x == 0) x = 1;
  for (size_t i = 0; i < kFrameBytes; i++) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    out[i] = static_cast<uint8_t>(x >> 11);
  }
}

int TestReport::percentOk() const {
  if (framesOut == 0) return 0;
  const int p = static_cast<int>((100u * framesOk + framesOut / 2u) / framesOut);
  return p > 100 ? 100 : p;
}

static int8_t clampInt8(int v) { return static_cast<int8_t>(v < -127 ? -127 : (v > 127 ? 127 : v)); }

void encodeTestReport(const TestReport& r, uint8_t* out) {
  out[0] = static_cast<uint8_t>(r.framesOut >> 8);
  out[1] = static_cast<uint8_t>(r.framesOut);
  out[2] = static_cast<uint8_t>(r.framesOk >> 8);
  out[3] = static_cast<uint8_t>(r.framesOk);
  out[4] = static_cast<uint8_t>(r.framesBad >> 8);
  out[5] = static_cast<uint8_t>(r.framesBad);
  out[6] = static_cast<uint8_t>(r.framesLost >> 8);
  out[7] = static_cast<uint8_t>(r.framesLost);
  out[8] = r.recovered;
  out[9] = static_cast<uint8_t>(clampInt8(r.rssiDbm));
  out[10] = static_cast<uint8_t>(clampInt8(r.snrDb));
  out[11] = r.snrValid ? 1 : 0;
}

bool decodeTestReport(const uint8_t* in, size_t len, TestReport& r) {
  if (!in || len != kTestReportBytes) return false;
  r.framesOut = static_cast<uint16_t>((in[0] << 8) | in[1]);
  r.framesOk = static_cast<uint16_t>((in[2] << 8) | in[3]);
  r.framesBad = static_cast<uint16_t>((in[4] << 8) | in[5]);
  r.framesLost = static_cast<uint16_t>((in[6] << 8) | in[7]);
  r.recovered = in[8];
  r.rssiDbm = static_cast<int8_t>(in[9]);
  r.snrDb = static_cast<int8_t>(in[10]);
  r.snrValid = in[11] != 0;
  return true;
}

void LinkTestRx::begin(uint16_t session) { *this = LinkTestRx(); session_ = session; }

void LinkTestRx::onFrame(const uint8_t* frame) {
  uint8_t expected[kFrameBytes];
  testFramePattern(session_, index_++, expected);
  if (memcmp(expected, frame, kFrameBytes) == 0) {
    ok_++;
  } else {
    bad_++;
  }
}

void LinkTestRx::onLost() {
  index_++;
  lost_++;
}

void LinkTestRx::onPacket(int rssiDbm, float snrDb, bool snrValid) {
  rssiSum_ += rssiDbm;
  packets_++;
  if (snrValid) {
    snrSum_ += static_cast<long>(std::lround(snrDb));
    snrPackets_++;
  }
}

TestReport LinkTestRx::report(uint8_t recoveredPackets) const {
  TestReport r;
  r.framesOk = ok_;
  r.framesBad = bad_;
  r.framesLost = lost_;
  r.framesOut = static_cast<uint16_t>(ok_ + bad_ + lost_);
  r.recovered = recoveredPackets;
  r.rssiDbm = packets_ ? static_cast<int>(std::lround(static_cast<double>(rssiSum_) / packets_)) : 0;
  r.snrValid = snrPackets_ > 0;
  r.snrDb = snrPackets_ ? static_cast<int>(std::lround(static_cast<double>(snrSum_) / snrPackets_)) : 0;
  return r;
}

void PowerSweep::begin(int maxDbm, int minDbm, int restoreDbm, uint32_t frames) {
  *this = PowerSweep();
  cur_ = maxDbm;
  min_ = minDbm;
  restore_ = restoreDbm;
  frames_ = frames > 0 ? frames : kSweepFrames;
  active_ = maxDbm >= minDbm;
}

bool PowerSweep::record(bool answered, const TestReport& r) {
  if (!active_) return false;
  if (n_ < kMaxSteps) {
    Step& s = steps_[n_++];
    s.dbm = static_cast<int8_t>(cur_);
    s.answered = answered;
    if (answered) {
      const uint32_t p = (100u * r.framesOk + frames_ / 2u) / frames_;
      s.percent = static_cast<uint8_t>(p > 100 ? 100 : p);
      s.rssiDbm = clampInt8(r.rssiDbm);
      s.snrDb = clampInt8(r.snrDb);
      s.snrValid = r.snrValid;
    }
  }
  const bool twoSilent = n_ >= 2 && !steps_[n_ - 1].answered && !steps_[n_ - 2].answered;
  if (cur_ - 1 < min_ || twoSilent || n_ >= kMaxSteps) {
    active_ = false;
    return false;
  }
  cur_--;
  return true;
}

int PowerSweep::minPerfectDbm() const {
  int best = -1;
  for (size_t i = 0; i < n_; i++) {
    if (!steps_[i].answered || steps_[i].percent < 100) break;
    best = steps_[i].dbm;
  }
  return best;
}

int PowerSweep::minAnsweredDbm() const {
  int best = -1;
  for (size_t i = 0; i < n_; i++) {
    if (steps_[i].answered && steps_[i].percent > 0) best = steps_[i].dbm;
  }
  return best;
}

}  // namespace lcr
