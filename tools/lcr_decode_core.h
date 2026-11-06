// Ricostruzione offline di una ricezione: dai pacchetti catturati con un SDR (byte in esadecimale) alle sessioni
// vocali e di prova, con la STESSA logica del ricevitore del firmware (RxStream: riassemblaggio, parita' XOR, frame persi).
// Solo intestazione, indipendente dalla scheda: lo usano tools/lcr_decode.cpp e tests/test_decode.cpp.
//
// Formato di una riga di cattura (una per pacchetto LoRa):
//   t=1730900000.123 crc=1 hex=0004d2...        (tools/sdr/lora_rx.py)
//   1 0004d2...                                  (crc 0/1, poi i byte)
//   0004d2...                                    (solo i byte, si assume crc valido)
//   rx msg: 0x0, 0x4, 0xd2, ...                  (uscita di gr-lora_sdr con la stampa in esadecimale)
// Le righe vuote e quelle che iniziano con '#' si ignorano.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "link_test.h"
#include "packet.h"
#include "protocol.h"
#include "rx_stream.h"

namespace lcr {
namespace decode {

struct Capture {
  bool hasTime = false;
  double tSec = 0.0;
  bool crcOk = true;
  std::vector<uint8_t> bytes;
};

struct Session {
  uint16_t id = 0;
  std::string callsign;
  bool isTest = false;
  uint32_t packets = 0;        // pacchetti accettati
  uint32_t frames = 0;         // frame consegnati (buoni)
  uint32_t framesLost = 0;     // frame persi e non recuperabili
  uint32_t recovered = 0;      // pacchetti ricostruiti con la parita'
  bool ended = false;          // visto il pacchetto di fine (OVER) o chiusa per tempo
  double startSec = 0.0;
  std::vector<uint8_t> c2;     // frame Codec2 1200 consecutivi (6 byte l'uno; i persi sono a zero)
  TestReport test;             // solo per le prove di collegamento
};

struct Reply {  // risposta (modo 3) a una prova
  uint16_t session = 0;
  std::string callsign;
  TestReport report;
};

struct Summary {
  size_t lines = 0, captures = 0, badCrc = 0, badPacket = 0, otherModes = 0;
  std::vector<Reply> replies;
};

inline int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

inline bool parseHexString(const std::string& s, std::vector<uint8_t>& out) {
  if (s.size() < 2 || s.size() % 2) return false;
  out.clear();
  for (size_t i = 0; i < s.size(); i += 2) {
    const int a = hexVal(s[i]), b = hexVal(s[i + 1]);
    if (a < 0 || b < 0) return false;
    out.push_back(static_cast<uint8_t>(a * 16 + b));
  }
  return true;
}

// Una riga di cattura. false se non contiene un pacchetto.
inline bool parseLine(const std::string& raw, Capture& c) {
  size_t a = raw.find_first_not_of(" \t\r\n");
  if (a == std::string::npos || raw[a] == '#') return false;
  c = Capture();

  // Stampa di gr-lora_sdr: "rx msg: 0x0, 0x4, 0xd2" (i byte piccoli senza zero iniziale).
  if (raw.find("0x") != std::string::npos && raw.find("hex=") == std::string::npos) {
    size_t p = 0;
    while ((p = raw.find("0x", p)) != std::string::npos) {
      p += 2;
      std::string h;
      while (p < raw.size() && h.size() < 2 && hexVal(raw[p]) >= 0) h += raw[p++];
      if (h.empty()) return false;
      c.bytes.push_back(static_cast<uint8_t>(std::strtoul(h.c_str(), nullptr, 16)));
    }
    return c.bytes.size() >= 5;
  }

  std::vector<std::string> tok;
  for (size_t i = a; i < raw.size();) {
    size_t j = raw.find_first_of(" \t\r\n", i);
    if (j == std::string::npos) j = raw.size();
    tok.push_back(raw.substr(i, j - i));
    i = raw.find_first_not_of(" \t\r\n", j);
    if (i == std::string::npos) break;
  }
  bool gotHex = false;
  for (size_t i = 0; i < tok.size(); i++) {
    const std::string& t = tok[i];
    if (t.rfind("t=", 0) == 0) {
      c.hasTime = true;
      c.tSec = std::atof(t.c_str() + 2);
    } else if (t.rfind("crc=", 0) == 0) {
      c.crcOk = t.size() > 4 && t[4] == '1';
    } else if (t.rfind("hex=", 0) == 0) {
      gotHex = parseHexString(t.substr(4), c.bytes);
    } else if (t.rfind("len=", 0) == 0) {
      // informativo
    } else if (t == "0" || t == "1") {
      if (i + 1 < tok.size()) c.crcOk = (t == "1");
    } else if (!gotHex && parseHexString(t, c.bytes)) {
      gotHex = true;
    }
  }
  return gotHex && c.bytes.size() >= 5;
}

inline std::vector<Capture> parseCaptures(const std::string& text, Summary* sum = nullptr) {
  std::vector<Capture> v;
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t e = text.find('\n', pos);
    if (e == std::string::npos) e = text.size();
    Capture c;
    if (sum) sum->lines++;
    if (parseLine(text.substr(pos, e - pos), c)) v.push_back(c);
    pos = e + 1;
    if (e == text.size()) break;
  }
  return v;
}

// Riproduce quello che farebbe la scheda ricevente (VoiceNode::rxTick) con questi pacchetti.
inline std::vector<Session> reassemble(const std::vector<Capture>& caps, Summary* sum = nullptr, bool acceptBadCrc = false) {
  std::vector<Session> out;
  const size_t n = caps.size();
  if (sum) sum->captures += n;
  if (n == 0) return out;

  bool allTimes = true;
  for (const auto& c : caps) allTimes = allTimes && c.hasTime;
  std::vector<uint32_t> t(n);
  const double t0 = caps[0].tSec;
  for (size_t i = 0; i < n; i++) {
    t[i] = allTimes ? static_cast<uint32_t>(std::max(0.0, (caps[i].tSec - t0) * 1000.0 + 0.5)) : static_cast<uint32_t>(i * 300);
  }
  std::vector<size_t> order(n);
  for (size_t i = 0; i < n; i++) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return t[a] < t[b]; });

  RxStream rx;
  LinkTestRx lt;
  Session cur;
  bool receiving = false, isTest = false;
  uint32_t now = t[order[0]], nextPlay = 0;
  size_t next = 0;
  const uint32_t lastT = t[order[n - 1]];

  auto finish = [&](bool ended) {
    cur.ended = ended;
    if (!cur.callsign.size() && rx.callsign()[0]) cur.callsign = rx.callsign();
    cur.recovered = rx.recoveredPackets();
    if (isTest) cur.test = lt.report(static_cast<uint8_t>(cur.recovered > 255 ? 255 : cur.recovered));
    out.push_back(cur);
    receiving = false;
  };

  auto feed = [&](const Capture& c) {
    if (!c.crcOk && !acceptBadCrc) {
      if (sum) sum->badCrc++;
      return;
    }
    Packet p;
    if (!decodePacket(c.bytes.data(), c.bytes.size(), p)) {
      if (sum) sum->badPacket++;
      return;
    }
    if (p.mode == kModeTestReport) {
      Reply r;
      if (decodeTestReport(p.payload, p.payloadLen, r.report)) {
        r.session = p.session;
        r.callsign = p.callsign;
        if (sum) sum->replies.push_back(r);
      }
      return;
    }
    if (p.mode != kModeCodec2_1200 && p.mode != kModeTest) {
      if (sum) sum->otherModes++;
      return;
    }
    if (receiving && (p.mode == kModeTest) != isTest) return;
    if (!rx.onPacket(p, now)) return;
    if (!receiving) {
      receiving = true;
      isTest = p.mode == kModeTest;
      cur = Session();
      cur.id = p.session;
      cur.isTest = isTest;
      cur.startSec = c.hasTime ? c.tSec : 0.0;
      nextPlay = now;
      if (isTest) lt.begin(p.session);
    }
    cur.packets++;
    if (p.hasCallsign()) cur.callsign = p.callsign;
  };

  auto step = [&]() {
    const RxStream::Out o = rx.nextFrame(now);
    switch (o.kind) {
      case RxStream::Kind::Wait:
        nextPlay = now;
        return;
      case RxStream::Kind::Frame:
        cur.frames++;
        if (isTest) {
          lt.onFrame(o.frame);
        } else {
          cur.c2.insert(cur.c2.end(), o.frame, o.frame + kFrameBytes);
        }
        break;
      case RxStream::Kind::Lost:
        cur.framesLost++;
        if (isTest) {
          lt.onLost();
        } else {
          cur.c2.insert(cur.c2.end(), kFrameBytes, 0);
        }
        break;
      case RxStream::Kind::End:
        finish(true);
        return;
    }
    nextPlay += kFrameMs;
    if (now > nextPlay + 5 * kFrameMs) nextPlay = now + kFrameMs;
  };

  while ((next < n || receiving) && now <= lastT + 60000) {
    if (!receiving && next < n && t[order[next]] > now) now = t[order[next]];  // salta i silenzi
    while (next < n && t[order[next]] <= now) feed(caps[order[next++]]);
    if (receiving && now >= nextPlay) step();
    now += 10;
  }
  if (receiving) finish(false);  // cattura finita a meta' trasmissione
  return out;
}

}  // namespace decode
}  // namespace lcr
