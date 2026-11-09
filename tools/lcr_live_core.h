// Ricezione in tempo reale dai pacchetti catturati con un SDR: la STESSA logica del ricevitore del firmware (RxStream:
// riassemblaggio, parita' XOR, frame persi), ma con l'orologio vero invece di uno simulato, e con i frame Codec2 consegnati
// a ritmo di 40 ms a un'uscita (tools/lcr_live.cpp li scrive sullo standard output, da dove vanno a c2dec e all'altoparlante).
// Solo intestazione, indipendente dalla scheda e dal sistema operativo: testato sul PC con un orologio finto (tests/test_live.cpp).
//
// Uso:  feed() a ogni pacchetto letto (con l'ora di arrivo in millisecondi) e tick() spesso (almeno ogni 10 ms).
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "lcr_decode_core.h"
#include "profile.h"

namespace lcr {
namespace decode {

class LiveRx {
 public:
  class Sink {
   public:
    virtual ~Sink() {}
    // Un frame Codec2 (kFrameBytes byte); lost = frame perso e non recuperabile (i byte sono a zero, come in lcr_decode).
    virtual void frame(const uint8_t* f, bool lost) = 0;
    // Riga di stato per l'operatore (nominativo, posizione, avanzamento, fine della trasmissione).
    virtual void info(const std::string& line) = 0;
  };

  // message: profilo Messaggio (la riproduzione parte a messaggio completo, come sulle schede); altrimenti Standard.
  LiveRx(Sink& sink, bool message = false, bool acceptBadCrc = false) : sink_(sink), acceptBad_(acceptBadCrc) {
    const Profile& p = profileFor(message ? kProfileMessage : kProfileStandard);
    RxStream::Config rc;
    rc.prebufferFrames = p.rxPrebufferFrames;
    rc.prebufferTimeoutMs = p.rxPrebufferTimeoutMs;
    rc.sessionTimeoutMs = p.rxSessionTimeoutMs;
    rx_.configure(rc);
  }

  bool receiving() const { return receiving_; }

  void feed(const Capture& c, uint32_t now) {
    if (!c.crcOk && !acceptBad_) {
      info("pacchetto con CRC errato scartato");
      return;
    }
    Packet p;
    if (!decodePacket(c.bytes.data(), c.bytes.size(), p)) return;  // non e' un pacchetto del progetto
    if (p.mode == kModeTestReport) {
      TestReport r;
      if (decodeTestReport(p.payload, p.payloadLen, r)) {
        char b[160];
        std::snprintf(b, sizeof(b), "risposta di %s alla prova %u: ok %u/%u, danneggiati %u, persi %u, RSSI %d dBm", p.callsign,
                      p.session, r.framesOk, r.framesOut, r.framesBad, r.framesLost, r.rssiDbm);
        info(b);
      }
      return;
    }
    if (p.mode != kModeCodec2_1200 && p.mode != kModeTest) return;
    if (receiving_ && (p.mode == kModeTest) != isTest_) return;
    if (!rx_.onPacket(p, now)) return;
    if (!receiving_) {
      receiving_ = true;
      isTest_ = p.mode == kModeTest;
      session_ = p.session;
      nextPlay_ = now;
      frames_ = lostFrames_ = 0;
      callsignShown_ = posShown_ = false;
      pctShown_ = 0;
      if (isTest_) lt_.begin(p.session);
      char b[96];
      std::snprintf(b, sizeof(b), "inizio %s, sessione %u", isTest_ ? "prova di collegamento" : "trasmissione", p.session);
      info(b);
    }
    progress();
  }

  void tick(uint32_t now) {
    if (receiving_ && now >= nextPlay_) step(now);
  }

 private:
  void info(const std::string& s) { sink_.info(s); }

  void progress() {
    if (!callsignShown_ && rx_.callsign()[0]) {
      callsignShown_ = true;
      info(std::string("da ") + rx_.callsign());
    }
    if (!posShown_ && rx_.position().valid) {
      posShown_ = true;
      char b[96];
      std::snprintf(b, sizeof(b), "posizione %.5f, %.5f (%s)", rx_.position().latE7 / 1e7, rx_.position().lonE7 / 1e7,
                    rx_.position().fromGps ? "GPS" : "fissa");
      info(b);
    }
    const size_t total = rx_.totalPackets();  // solo i messaggi dichiarano la lunghezza
    if (total && !isTest_) {
      size_t pct = rx_.highestSeq() == 0 ? 1 : (rx_.highestSeq() + 1) * 100 / total;
      if (pct > 100) pct = 100;
      // Una riga ogni 10%, e la prima e l'ultima
      const int mark = pct >= 100 ? 100 : (pct < 10 ? (pct >= 1 ? 1 : 0) : static_cast<int>(pct / 10 * 10));
      if (mark > pctShown_) {
        pctShown_ = mark;
        char b[48];
        std::snprintf(b, sizeof(b), "avanzamento %d%%", mark);
        info(b);
      }
    }
  }

  void step(uint32_t now) {
    const RxStream::Out o = rx_.nextFrame(now);
    switch (o.kind) {
      case RxStream::Kind::Wait:
        nextPlay_ = now;
        return;
      case RxStream::Kind::Frame:
        frames_++;
        if (isTest_) lt_.onFrame(o.frame);
        else sink_.frame(o.frame, false);
        break;
      case RxStream::Kind::Lost:
        lostFrames_++;
        if (isTest_) {
          lt_.onLost();
        } else {
          const uint8_t zero[kFrameBytes] = {0};
          sink_.frame(zero, true);
        }
        break;
      case RxStream::Kind::End:
        finish();
        return;
    }
    nextPlay_ += kFrameMs;
    if (now > nextPlay_ + 5 * kFrameMs) nextPlay_ = now + kFrameMs;
  }

  void finish() {
    char b[200];
    const uint32_t rec = rx_.recoveredPackets();
    if (isTest_) {
      const TestReport r = lt_.report(static_cast<uint8_t>(rec > 255 ? 255 : rec));
      std::snprintf(b, sizeof(b), "PROVA DI COLLEGAMENTO da %s: ok %u/%u (%d%%), danneggiati %u, persi %u, pacchetti recuperati %u%s",
                    rx_.callsign()[0] ? rx_.callsign() : "-", r.framesOk, r.framesOut, r.percentOk(), r.framesBad, r.framesLost, rec,
                    r.complete() ? "" : " [parziale]");
    } else {
      std::snprintf(b, sizeof(b), "fine: da %s, %u frame (%.1f s), persi %u, pacchetti recuperati %u", rx_.callsign()[0] ? rx_.callsign() : "-",
                    frames_ + lostFrames_, (frames_ + lostFrames_) * kFrameMs / 1000.0, lostFrames_, rec);
    }
    info(b);
    receiving_ = false;
  }

  Sink& sink_;
  bool acceptBad_;
  RxStream rx_;
  LinkTestRx lt_;
  bool receiving_ = false;
  bool isTest_ = false;
  uint16_t session_ = 0;
  uint32_t nextPlay_ = 0;
  uint32_t frames_ = 0, lostFrames_ = 0;
  bool callsignShown_ = false, posShown_ = false;
  int pctShown_ = 0;
};

}  // namespace decode
}  // namespace lcr
