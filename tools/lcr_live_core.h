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
    // Un frame Codec2 (rx_.frameBytes() byte); lost = frame perso e non recuperabile (i byte sono a zero, come in lcr_decode).
    virtual void frame(const uint8_t* f, size_t n, bool lost) = 0;  // n = byte del frame (6 a 1200, 16 a 3200)
    // Riga di stato per l'operatore (nominativo, posizione, avanzamento, fine della trasmissione).
    virtual void info(const std::string& line) = 0;
  };

  // message: profilo Messaggio (la riproduzione parte a messaggio completo, come sulle schede); altrimenti Standard.
  // leadMs: anticipo con cui si consegnano i frame rispetto al ritmo reale. I primi frame escono in blocco e poi uno ogni
  // 40 ms: chi riproduce (aplay) ha un cuscinetto di leadMs contro i piccoli ritardi e non va in "underrun" (vuoti nell'audio).
  // Costa leadMs di ritardo in meno, non in piu': l'audio parte prima, non dopo. 0 = ritmo reale esatto.
  LiveRx(Sink& sink, bool message = false, bool acceptBadCrc = false, uint32_t leadMs = 240)
      : sink_(sink), acceptBad_(acceptBadCrc), lead_(leadMs) {
    const Profile& p = profileFor(message ? kProfileMessage : kProfileRange);
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
    if (p.mode != kModeCodec2_1200 && p.mode != kModeCodec2_3200 && p.mode != kModeTest) return;
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
      const char* what = isTest_ ? "prova di collegamento"
                                 : (p.mode == kModeCodec2_3200 ? "trasmissione Standard (Codec2 3200)" : "trasmissione (Codec2 1200)");
      std::snprintf(b, sizeof(b), "inizio %s, sessione %u", what, p.session);
      info(b);
    }
    progress();
  }

  void tick(uint32_t now) {
    // Piu' frame nello stesso giro solo se si e' in anticipo (i primi, per il cuscinetto); mai un ciclo senza fine.
    for (int guard = 0; guard < 16 && receiving_ && now >= nextPlay_; guard++) {
      if (!step(now)) break;
    }
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

  // true se ha consegnato un frame (o chiuso la trasmissione): si puo' provare subito con il successivo.
  bool step(uint32_t now) {
    const RxStream::Out o = rx_.nextFrame(now);
    switch (o.kind) {
      case RxStream::Kind::Wait:
        nextPlay_ = now;
        return false;
      case RxStream::Kind::Frame:
        frames_++;
        if (isTest_) lt_.onFrame(o.frame);
        else sink_.frame(o.frame, rx_.frameBytes(), false);
        break;
      case RxStream::Kind::Lost:
        lostFrames_++;
        if (isTest_) {
          lt_.onLost();
        } else {
          const uint8_t zero[kMaxFrameBytes] = {0};
          sink_.frame(zero, rx_.frameBytes(), true);
        }
        break;
      case RxStream::Kind::End:
        finish();
        return false;
    }
    if (!isTest_ && frames_ + lostFrames_ == 1) {
      nextPlay_ = now + kFrameMs - lead_;  // primo frame: il cuscinetto parte da qui
    } else {
      nextPlay_ += kFrameMs;
    }
    if (now > nextPlay_ + 5 * kFrameMs + lead_) nextPlay_ = now + kFrameMs;  // troppo indietro: si riparte dal ritmo reale
    return true;
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
  uint32_t lead_;
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
