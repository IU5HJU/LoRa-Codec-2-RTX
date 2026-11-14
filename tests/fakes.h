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
  std::vector<std::vector<uint8_t>> log[2];  // pacchetti inviati da ciascuno, nell'ordine
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
    air_.log[id_].push_back(std::vector<uint8_t>(d, d + n));
    if (air_.drop && air_.drop(id_, idx)) return true;
    air_.inbox[1 - id_].push_back(std::vector<uint8_t>(d, d + n));
    return true;
  }
  void startReceive() override {}
  bool setMeshcom(bool on) override {
    meshcom = on;
    meshcomSwitches++;
    return !failMeshcom;
  }
  bool setParams(const lcr::Profile& prof, uint32_t freqKhz) override {
    if (failParams) return false;
    params.push_back({prof.id, freqKhz});
    curId = prof.id;
    return true;
  }
  struct ParamChange {
    uint8_t id;
    uint32_t khz;
  };
  std::vector<ParamChange> params;
  int curId = -1;  // profilo con cui la radio e' configurata (-1 = mai cambiato)
  bool setPower(int dbm) override {
    powers.push_back(dbm);
    return true;
  }
  std::vector<int> powers;
  bool fskBegin(float mhz, float kbps, float devKhz, int ramp, int shape) override {
    (void)ramp;
    (void)shape;
    if (failFsk) return false;
    fskBegins++;
    fskMhz = mhz;
    fskKbps = kbps;
    fskDev = devKhz;
    inFsk = true;
    return true;
  }
  bool fskSend(uint8_t pattern, uint16_t bytes) override {
    fskBursts.push_back({pattern, bytes});
    return inFsk;
  }
  void fskEnd() override {
    inFsk = false;
    fskEnds++;
  }
  struct FskBurst {
    uint8_t pattern;
    uint16_t bytes;
  };
  std::vector<FskBurst> fskBursts;
  bool inFsk = false, failFsk = false;
  int fskBegins = 0, fskEnds = 0;
  float fskMhz = 0, fskKbps = 0, fskDev = 0;
  bool failParams = false;
  bool meshcom = false;
  int meshcomSwitches = 0;
  bool failMeshcom = false;
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
  explicit Codec(size_t frameBytes = lcr::kFrameBytes) : fb_(frameBytes) {}
  void encode(const int16_t* pcm, uint8_t* f) override {
    f[0] = static_cast<uint8_t>(pcm[0] & 0xFF);
    f[1] = static_cast<uint8_t>((pcm[0] >> 8) & 0xFF);
    for (size_t i = 2; i < fb_; i++) f[i] = static_cast<uint8_t>(0x5A + i);
  }
  void decode(const uint8_t* f, int16_t* pcm) override {
    memset(pcm, 0, lcr::kFrameSamples * sizeof(int16_t));
    pcm[0] = static_cast<int16_t>(f[0] | (f[1] << 8));
    for (size_t i = 2; i < fb_; i++) {
      if (f[i] != static_cast<uint8_t>(0x5A + i)) pcm[1] = 1;  // payload alterato
    }
  }
  size_t fb_;
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
  void rxPlaybackStarted() override { playbackStarts++; }
  int playbackStarts = 0;
  void rxCallsign(const char* cs) override { callsign = cs; }
  void rxPosition(const lcr::Position& p) override {
    positions++;
    position = p;
  }
  void testPeerPosition(const lcr::Position& p) override {
    peerPositions++;
    peerPosition = p;
  }
  int peerPositions = 0;
  lcr::Position peerPosition;
  void txProgress(int p) override { txPercents.push_back(p); }
  void rxProgress(int p) override { rxPercents.push_back(p); }
  std::vector<int> txPercents, rxPercents;
  int positions = 0;
  lcr::Position position;
  void rxEnded(const char*, uint32_t lost, uint32_t rec) override {
    rxEnds++;
    lostFrames = lost;
    recovered = rec;
  }
  void messageReceived(bool isText, const char* cs) override {
    received.push_back({isText, cs});
  }
  void repeaterBusy(const char* cs, uint16_t sec) override { busyEvents.push_back({cs, sec}); }
  struct BusyEv {
    std::string callsign;
    uint16_t seconds;
  };
  std::vector<BusyEv> busyEvents;
  void sysopReply(const char* cs, const lcr::SysopReply& r) override { sysopReplies.push_back({cs, r}); }
  struct SysRep {
    std::string callsign;
    lcr::SysopReply r;
  };
  std::vector<SysRep> sysopReplies;
  void beaconReceived(const char* cs) override { beacons.push_back(cs); }
  std::vector<std::string> beacons;
  void rxViaRepeater() override { viaRepeater++; }
  int viaRepeater = 0;
  void messageOpened(bool isText) override { opened.push_back(isText); }
  void messageClosed() override { closed++; }
  void meshcomChanged(bool on) override { meshcomEvents.push_back(on); }
  std::vector<bool> meshcomEvents;
  struct Msg {
    bool isText;
    std::string callsign;
  };
  std::vector<Msg> received;
  std::vector<bool> opened;
  int closed = 0;
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
  void powerSweepStep(const lcr::PowerSweep::Step& s, size_t) override { sweepSteps.push_back(s); }
  void powerSweepDone(const lcr::PowerSweep& sw, bool aborted) override {
    sweepDone++;
    sweepAborted = aborted;
    sweepMinPerfect = sw.minPerfectDbm();
    sweepMinAnswered = sw.minAnsweredDbm();
  }
  void powerSweepSummary(const char* cs, const lcr::SweepSummary& s) override {
    summaries.push_back(s);
    summaryCall = cs;
  }
  void powerSweepStarted(int) override { sweepStarts++; }
  std::vector<lcr::SweepSummary> summaries;
  std::string summaryCall;
  int sweepStarts = 0;
  std::vector<lcr::PowerSweep::Step> sweepSteps;
  int sweepDone = 0, sweepMinPerfect = -2, sweepMinAnswered = -2;
  bool sweepAborted = false;
  int txStarts = 0, txEnds = 0, denied = 0, rxStarts = 0, rxEnds = 0;
  bool limitHit = false;
  lcr::DenyReason lastDeny = lcr::DenyReason::Receiving;
  std::string callsign;
  uint32_t lostFrames = 0, recovered = 0;
};

}  // namespace fake
