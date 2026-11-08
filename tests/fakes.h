// Implementazioni finte delle interfacce hardware per i test sul PC.
#pragma once

#include <cstring>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "interfaces.h"
#include "protocol.h"

namespace fake {

// Canale radio condiviso tra due nodi (0 e 1).
struct Air {
  std::deque<std::vector<uint8_t>> inbox[2];
  int sent[2] = {0, 0};
  const uint32_t* clock = nullptr;           // se impostato, registra l'istante di ogni invio
  std::vector<uint32_t> sentAt[2];
  bool busy = false;
  // Se ritorna true il pacchetto numero idx inviato da 'from' viene perso.
  std::function<bool(int from, int idx)> drop;
};

class Radio : public lcr::IRadio {
 public:
  Radio(Air& a, int id) : air_(a), id_(id) {}
  bool channelBusy() override { return air_.busy; }
  bool send(const uint8_t* d, size_t n) override {
    const int idx = air_.sent[id_]++;
    if (air_.clock) air_.sentAt[id_].push_back(*air_.clock);
    if (air_.drop && air_.drop(id_, idx)) return true;
    air_.inbox[1 - id_].push_back(std::vector<uint8_t>(d, d + n));
    return true;
  }
  void startReceive() override {}
  size_t receive(uint8_t* buf, size_t cap, int* rssi) override {
    auto& q = air_.inbox[id_];
    if (q.empty()) return 0;
    const size_t n = q.front().size() < cap ? q.front().size() : cap;
    memcpy(buf, q.front().data(), n);
    q.pop_front();
    if (rssi) *rssi = -90;
    return n;
  }

 private:
  Air& air_;
  int id_;
};

// Il "codec" porta un contatore nel primo campione: encode/decode sono
// reversibili e permettono di verificare l'ordine dei frame.
class Codec : public lcr::ICodec {
 public:
  void encode(const int16_t* pcm, uint8_t* f) override {
    f[0] = static_cast<uint8_t>(pcm[0] & 0xFF);
    f[1] = static_cast<uint8_t>((pcm[0] >> 8) & 0xFF);
    memset(f + 2, 0x5A, 4);
  }
  void decode(const uint8_t* f, int16_t* pcm) override {
    memset(pcm, 0, lcr::kFrameSamples * sizeof(int16_t));
    pcm[0] = static_cast<int16_t>(f[0] | (f[1] << 8));
  }
};

// Microfono: produce un frame ogni 40 ms di tempo simulato, con contatore 1,2,3...
class Mic : public lcr::IAudioIn {
 public:
  explicit Mic(const uint32_t& clock) : clock_(clock) {}
  void start() override {
    running_ = true;
    startMs_ = clock_;
    produced_ = 0;
  }
  void stop() override { running_ = false; }
  bool readFrame(int16_t* pcm) override {
    if (!running_) return false;
    if (produced_ >= (clock_ - startMs_) / lcr::kFrameMs) return false;
    memset(pcm, 0, lcr::kFrameSamples * sizeof(int16_t));
    pcm[0] = static_cast<int16_t>(++produced_);
    return true;
  }
  uint32_t produced() const { return produced_; }

 private:
  const uint32_t& clock_;
  bool running_ = false;
  uint32_t startMs_ = 0;
  uint32_t produced_ = 0;
};

// Altoparlante: registra il contatore dei frame riprodotti (-1 = frame perso).
class Speaker : public lcr::IAudioOut {
 public:
  void playFrame(const int16_t* pcm) override { played.push_back(pcm[0]); }
  void playLost() override { played.push_back(-1); }
  std::vector<int> played;
};

class Notifier : public lcr::INotifier {
 public:
  void txStarted() override { txStarts++; }
  void txEnded(bool limit) override {
    txEnds++;
    limitHit = limitHit || limit;
  }
  void pttDenied(lcr::DenyReason r) override {
    denied++;
    lastDeny = r;
  }
  void rxStarted() override { rxStarts++; }
  void rxCallsign(const char* cs) override { callsign = cs; }
  void rxEnded(const char*, uint32_t lost, uint32_t rec) override {
    rxEnds++;
    lostFrames = lost;
    recovered = rec;
  }
  void testModeChanged(bool on) override { testModeChanges.push_back(on); }
  void recPhaseChanged(int p) override { recPhases.push_back(p); }
  std::vector<int> recPhases;
  std::vector<bool> testModeChanges;
  void linkTestReport(bool fromRemote, const char* cs, const lcr::TestReport& r) override {
    reports.push_back({fromRemote, cs, r});
  }
  struct Rep {
    bool fromRemote;
    std::string callsign;
    lcr::TestReport r;
  };
  std::vector<Rep> reports;
  int txStarts = 0, txEnds = 0, denied = 0, rxStarts = 0, rxEnds = 0;
  bool limitHit = false;
  lcr::DenyReason lastDeny = lcr::DenyReason::Receiving;
  std::string callsign;
  uint32_t lostFrames = 0, recovered = 0;
};

}  // namespace fake
