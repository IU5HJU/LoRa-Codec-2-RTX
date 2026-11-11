#include <cstring>
#include <vector>

#include "fakes.h"
#include "rx_monitor.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;

// Frame con il contatore nei primi due byte, come il codec finto (pcm[0] = contatore).
static std::vector<Packet> makeTx(int nFrames, uint8_t mode = kModeCodec2_1200) {
  TxSession tx("IU5HJU", 0x4321, true, mode);
  std::vector<Packet> out;
  tx.begin();
  for (int i = 1; i <= nFrames; i++) {
    uint8_t f[kMaxFrameBytes];
    memset(f, 0, sizeof(f));
    f[0] = static_cast<uint8_t>(i & 0xFF);
    f[1] = static_cast<uint8_t>(i >> 8);
    for (size_t k = 2; k < frameBytesFor(mode); k++) f[k] = static_cast<uint8_t>(0x5A + k);
    CHECK(tx.pushFrame(f));
    Packet p;
    while (tx.popPacket(p)) out.push_back(p);
  }
  tx.finish();
  Packet p;
  while (tx.popPacket(p)) out.push_back(p);
  return out;
}

static std::vector<int> seq(int n) {
  std::vector<int> v;
  for (int i = 1; i <= n; i++) v.push_back(i);
  return v;
}

// Consegna i pacchetti (tranne quelli in 'drop') uno ogni 'gapMs' e fa girare il monitor fino alla fine.
static void run(RxMonitor& m, const std::vector<Packet>& pk, uint32_t& now, uint32_t gapMs = 100, int drop1 = -1, int drop2 = -1) {
  for (size_t i = 0; i < pk.size(); i++) {
    if (static_cast<int>(i) != drop1 && static_cast<int>(i) != drop2) m.onPacket(pk[i], now);
    for (uint32_t t = 0; t < gapMs; t += 10) {
      m.tick(now);
      now += 10;
    }
  }
  for (int i = 0; i < 4000 && m.active(); i++) {
    m.tick(now);
    now += 10;
  }
}

static void testVoice() {
  fake::Codec codec(kFrameBytes);
  fake::Speaker spk;
  RxMonitor m(kProfileStandard, codec, spk);
  uint32_t now = 0;
  CHECK(!m.active());
  const auto pk = makeTx(80);
  run(m, pk, now);
  CHECK(spk.played == seq(80));  // tutta la voce, in ordine
  CHECK(!m.active() && m.sessionsHeard() == 1 && m.framesPlayed() == 80 && m.framesLost() == 0);
  CHECK(strcmp(m.callsign(), "IU5HJU") == 0);
  // una seconda trasmissione si ascolta di nuovo
  spk.played.clear();
  auto second = makeTx(30);
  for (auto& p : second) p.session = 0x1111;  // ogni trasmissione ha la sua sessione
  run(m, second, now);
  CHECK(spk.played == seq(30) && m.sessionsHeard() == 2);
}

static void testLossRecovered() {
  fake::Codec codec(kFrameBytes);
  fake::Speaker spk;
  RxMonitor m(kProfileStandard, codec, spk);
  uint32_t now = 0;
  const auto pk = makeTx(80);
  run(m, pk, now, 100, 2);  // un pacchetto dati perso nel primo gruppo: la parita' lo recupera
  CHECK(spk.played == seq(80));
}

static void testNotVoice() {
  fake::Codec codec(kFrameBytes);
  fake::Speaker spk;
  RxMonitor m(kProfileStandard, codec, spk);
  uint32_t now = 0;
  // testo (modo 7), prova (modo 2) e 3200 con il profilo Standard: ignorati
  Packet t;
  t.mode = kModeText;
  t.session = 9;
  t.flags = kFlagStart | kFlagEnd | kFlagCallsign;
  strcpy(t.callsign, "IU5HJU");
  memcpy(t.payload, "ciao", 4);
  t.payloadLen = 4;
  CHECK(!m.onPacket(t, now) && !m.active());
  for (const Packet& p : makeTx(10, kModeCodec2_3200)) CHECK(!m.onPacket(p, now));
  m.tick(now + 1000);
  CHECK(spk.played.empty() && m.sessionsHeard() == 0);
}

static void testOtherSessionIgnoredWhilePlaying() {
  fake::Codec codec(kFrameBytes);
  fake::Speaker spk;
  RxMonitor m(kProfileStandard, codec, spk);
  uint32_t now = 0;
  auto a = makeTx(40);
  auto b = makeTx(40);
  for (auto& p : b) p.session = 0x7777;
  CHECK(m.onPacket(a[0], now));
  CHECK(!m.onPacket(b[0], now + 10));  // un'altra stazione mentre si ascolta la prima: non si mescola
}

static void testMessageProfile() {
  // Profilo Messaggio: nulla prima della fine del messaggio, poi si ascolta tutto
  fake::Codec codec(kFrameBytes);
  fake::Speaker spk;
  RxMonitor m(kProfileMessage, codec, spk);
  uint32_t now = 0;
  const auto pk = makeTx(40);
  size_t endIdx = 0;
  for (size_t i = 0; i < pk.size(); i++) if (pk[i].flags & kFlagEnd) endIdx = i;
  for (size_t i = 0; i < endIdx; i++) {
    m.onPacket(pk[i], now);
    for (int t = 0; t < 300; t++) {  // 3 s fra un pacchetto e l'altro
      m.tick(now);
      now += 10;
    }
  }
  CHECK(spk.played.empty());  // il messaggio non e' ancora finito
  m.onPacket(pk[endIdx], now);
  for (size_t i = endIdx + 1; i < pk.size(); i++) m.onPacket(pk[i], now);
  for (int i = 0; i < 4000 && m.active(); i++) {
    m.tick(now);
    now += 10;
  }
  CHECK(spk.played == seq(40));
}

int main() {
  testVoice();
  testLossRecovered();
  testNotVoice();
  testOtherSessionIgnoredWhilePlaying();
  testMessageProfile();
  TEST_MAIN_END();
}
