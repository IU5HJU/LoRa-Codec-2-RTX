// Ricostruzione offline dei pacchetti catturati (tools/lcr_decode_core.h).
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

#include "../tools/lcr_decode_core.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;
using namespace lcr::decode;

static std::string toHex(const uint8_t* d, size_t n) {
  static const char* h = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; i++) {
    s += h[d[i] >> 4];
    s += h[d[i] & 15];
  }
  return s;
}

// Frame di prova numero i: contenuto riconoscibile.
static void voiceFrame(uint32_t i, uint8_t* f) {
  for (size_t k = 0; k < kMaxFrameBytes; k++) f[k] = static_cast<uint8_t>(i * 7 + k * 31 + 1);
}

struct Gen {
  std::vector<std::vector<uint8_t>> pkts;
  std::vector<std::vector<uint8_t>> frames;  // frame trasmessi
};

static Gen makeSession(uint16_t id, int nframes, uint8_t mode, bool fec = true) {
  Gen g;
  TxSession tx("IU5HJU", id, fec, mode);
  auto drain = [&]() {
    Packet p;
    while (tx.popPacket(p)) {
      uint8_t buf[kMaxWireBytes];
      const size_t n = encodePacket(p, buf, sizeof(buf));
      g.pkts.emplace_back(buf, buf + n);
    }
  };
  tx.begin();
  drain();
  for (int i = 0; i < nframes; i++) {
    uint8_t f[kMaxFrameBytes];
    if (mode == kModeTest) {
      testFramePattern(id, static_cast<uint32_t>(i), f);
    } else {
      voiceFrame(static_cast<uint32_t>(i), f);
    }
    g.frames.emplace_back(f, f + frameBytesFor(mode));
    tx.pushFrame(f);
    drain();  // la coda di trasmissione tiene 8 pacchetti: si svuota man mano, come fa il nodo
  }
  tx.finish();
  drain();
  return g;
}

static std::string capture(const Gen& g, const std::vector<int>& drop, double t0 = 1000.0, bool withTime = true, int crc = 1) {
  std::ostringstream o;
  for (size_t i = 0; i < g.pkts.size(); i++) {
    if (std::find(drop.begin(), drop.end(), static_cast<int>(i)) != drop.end()) continue;
    if (withTime) o << "t=" << (t0 + i * 0.35) << " ";
    o << "crc=" << crc << " hex=" << toHex(g.pkts[i].data(), g.pkts[i].size()) << "\n";
  }
  return o.str();
}

static void testParse() {
  Capture c;
  CHECK(parseLine("t=12.5 crc=1 hex=0004d2aa55", c) && c.hasTime && c.tSec == 12.5 && c.crcOk && c.bytes.size() == 5 && c.bytes[2] == 0xd2);
  CHECK(parseLine("0 0004d2aa55", c) && !c.crcOk && c.bytes.size() == 5);
  CHECK(parseLine("1 0004d2aa55", c) && c.crcOk);
  CHECK(parseLine("0004d2aa55", c) && c.crcOk && !c.hasTime && c.bytes.size() == 5);
  CHECK(parseLine("rx msg: 0x0, 0x4, 0xd2, 0xa, 0x55, 0x1", c) && c.bytes.size() == 6 && c.bytes[3] == 0x0a);  // gr-lora_sdr
  CHECK(!parseLine("", c));
  CHECK(!parseLine("   ", c));
  CHECK(!parseLine("# commento", c));
  CHECK(!parseLine("CRC valid!", c));
  CHECK(!parseLine("hex=zz", c));
  CHECK(!parseLine("0004", c));  // troppo corto per un pacchetto
}

static void testVoiceClean() {
  const Gen g = makeSession(1234, 100, kModeCodec2_1200);
  Summary sum;
  const auto caps = parseCaptures(capture(g, {}), &sum);
  CHECK(caps.size() == g.pkts.size());
  const auto ss = reassemble(caps, &sum);
  CHECK(ss.size() == 1);
  const Session& s = ss[0];
  CHECK(s.id == 1234 && s.callsign == "IU5HJU" && !s.isTest && s.ended);
  CHECK(s.frames == 100 && s.framesLost == 0 && s.recovered == 0);
  CHECK(s.c2.size() == 100 * kFrameBytes);
  bool same = s.c2.size() == 100 * kFrameBytes;
  for (size_t i = 0; same && i < 100; i++) same = memcmp(&s.c2[i * kFrameBytes], g.frames[i].data(), kFrameBytes) == 0;
  CHECK(same);  // i frame escono identici e nello stesso ordine
}

static void testLossesAndParity() {
  const Gen g = makeSession(77, 200, kModeCodec2_1200);
  // pacchetto 2 (dato): recuperato dalla parita'; pacchetti 6 e 7 (due dati dello stesso gruppo): persi.
  const auto ss = reassemble(parseCaptures(capture(g, {2, 6, 7})));
  CHECK(ss.size() == 1);
  const Session& s = ss[0];
  CHECK(s.recovered >= 1);
  CHECK(s.framesLost == 24);
  CHECK(s.c2.size() == 200 * kFrameBytes);  // i persi occupano il loro posto, a zero
  size_t zeros = 0;
  for (size_t i = 0; i < 200; i++) {
    bool z = true;
    for (size_t k = 0; k < kFrameBytes; k++) z = z && s.c2[i * kFrameBytes + k] == 0;
    if (z) zeros++;
    else CHECK(memcmp(&s.c2[i * kFrameBytes], g.frames[i].data(), kFrameBytes) == 0);
  }
  CHECK(zeros == 24);
}

static void testVoice3200() {
  const Gen g = makeSession(555, 200, kModeCodec2_3200);
  const size_t fb = 16;
  // pacchetto 2 recuperato dalla parita', pacchetti 6 e 7 persi (24 frame)
  const auto ss = reassemble(parseCaptures(capture(g, {2, 6, 7})));
  CHECK(ss.size() == 1);
  const Session& s = ss[0];
  CHECK(s.mode == kModeCodec2_3200 && s.frameBytes == fb && s.recovered >= 1 && s.framesLost == 24);
  CHECK(s.c2.size() == 200 * fb);
  size_t zeros = 0;
  for (size_t i = 0; i < 200; i++) {
    bool z = true;
    for (size_t k = 0; k < fb; k++) z = z && s.c2[i * fb + k] == 0;
    if (z) zeros++;
    else CHECK(memcmp(&s.c2[i * fb], g.frames[i].data(), fb) == 0);
  }
  CHECK(zeros == 24);
}

static void testTestSession() {
  const Gen g = makeSession(4321, 300, kModeTest);
  auto ss = reassemble(parseCaptures(capture(g, {})));
  CHECK(ss.size() == 1 && ss[0].isTest && ss[0].c2.empty());  // le prove non producono audio
  CHECK(ss[0].test.perfect() && ss[0].test.framesOk == 300 && ss[0].test.percentOk() == 100);
  ss = reassemble(parseCaptures(capture(g, {6, 7})));
  CHECK(ss.size() == 1 && ss[0].test.framesLost == 24 && ss[0].test.framesBad == 0 && !ss[0].test.perfect());
}

static void testSessionsAndNoise() {
  const Gen a = makeSession(10, 50, kModeCodec2_1200);
  const Gen b = makeSession(11, 50, kModeCodec2_1200);
  std::string text = "# cattura di prova\n\n" + capture(a, {}, 1000.0) + "rumore senza senso\n" + capture(b, {}, 1030.0);
  text += "t=1100 crc=0 hex=" + toHex(a.pkts[0].data(), a.pkts[0].size()) + "\n";  // CRC errato: scartato
  text += "t=1101 crc=1 hex=ffffffffff\n";                                          // non e' un pacchetto nostro
  Summary sum;
  const auto ss = reassemble(parseCaptures(text, &sum), &sum);
  CHECK(ss.size() == 2);
  CHECK(ss[0].id == 10 && ss[1].id == 11 && ss[0].frames == 50 && ss[1].frames == 50);
  CHECK(sum.badCrc == 1 && sum.badPacket == 1);
}

static void testNoTimestamps() {
  const Gen g = makeSession(5, 60, kModeCodec2_1200);
  const auto ss = reassemble(parseCaptures(capture(g, {}, 0, /*withTime=*/false)));
  CHECK(ss.size() == 1 && ss[0].frames == 60 && ss[0].framesLost == 0);
}

static void testTruncated() {
  const Gen g = makeSession(6, 100, kModeCodec2_1200);
  // La cattura si ferma a meta': la sessione viene comunque restituita, segnata come interrotta.
  std::string part;
  std::istringstream in(capture(g, {}));
  std::string line;
  for (int i = 0; i < 10 && std::getline(in, line); i++) part += line + "\n";
  const auto ss = reassemble(parseCaptures(part));
  CHECK(ss.size() == 1);
  CHECK(ss[0].frames > 0);
}

static void testReplyAndOtherModes() {
  TestReport r;
  r.framesOut = 300;
  r.framesOk = 299;
  r.framesLost = 1;
  r.rssiDbm = -77;
  Packet p;
  p.mode = kModeTestReport;
  p.session = 4321;
  p.flags = kFlagStart | kFlagEnd;
  strcpy(p.callsign, "IZ5ABC");
  p.payloadLen = kTestReportBytes;
  encodeTestReport(r, p.payload);
  uint8_t buf[kMaxWireBytes];
  const size_t n = encodePacket(p, buf, sizeof(buf));
  Summary sum;
  const auto caps = parseCaptures("t=1 crc=1 hex=" + toHex(buf, n) + "\n", &sum);
  const auto ss = reassemble(caps, &sum);
  CHECK(ss.empty() && sum.replies.size() == 1);
  CHECK(sum.replies[0].callsign == "IZ5ABC" && sum.replies[0].session == 4321 && sum.replies[0].report.framesOk == 299);
}

int main() {
  testParse();
  testVoiceClean();
  testLossesAndParity();
  testVoice3200();
  testTestSession();
  testSessionsAndNoise();
  testNoTimestamps();
  testTruncated();
  testReplyAndOtherModes();
  TEST_MAIN_END();
}
