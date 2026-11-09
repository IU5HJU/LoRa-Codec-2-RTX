// Ricezione in tempo reale (tools/lcr_live_core.h) con un orologio finto.
#include <cstring>
#include <string>
#include <vector>

#include "../tools/lcr_live_core.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;
using namespace lcr::decode;

namespace {

struct Out : LiveRx::Sink {
  std::vector<std::vector<uint8_t>> frames;
  std::vector<bool> lost;
  std::vector<uint32_t> at;  // ora di ogni frame
  std::vector<std::string> lines;
  uint32_t* clock = nullptr;
  void frame(const uint8_t* f, bool l) override {
    frames.emplace_back(f, f + kFrameBytes);
    lost.push_back(l);
    at.push_back(clock ? *clock : 0);
  }
  void info(const std::string& s) override { lines.push_back(s); }
  bool has(const char* part) const {
    for (const auto& l : lines)
      if (l.find(part) != std::string::npos) return true;
    return false;
  }
};

void voiceFrame(uint32_t i, uint8_t* f) {
  for (size_t k = 0; k < kFrameBytes; k++) f[k] = static_cast<uint8_t>(i * 7 + k * 31 + 1);
}

struct Stream {
  std::vector<std::vector<uint8_t>> pkts;
  std::vector<std::vector<uint8_t>> frames;
};

Stream makeStream(int nframes, bool withTotal, bool withPosition) {
  Stream g;
  TxSession tx("IU5HJU", 4242, true);
  if (withTotal) tx.setExpectedTotal(TxSession::expectedPackets(nframes, true));
  if (withPosition) {
    Position p;
    p.valid = true;
    p.latE7 = 437696000;
    p.lonE7 = 112558000;
    tx.setPosition(p);
  }
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
    uint8_t f[kFrameBytes];
    voiceFrame(static_cast<uint32_t>(i), f);
    g.frames.emplace_back(f, f + kFrameBytes);
    tx.pushFrame(f);
    drain();
  }
  tx.finish();
  drain();
  return g;
}

Capture cap(const std::vector<uint8_t>& b, bool crc = true) {
  Capture c;
  c.bytes = b;
  c.crcOk = crc;
  return c;
}

// Fa girare l'orologio: i pacchetti arrivano a intervalli di 'gapMs' (tranne quelli in 'drop'); tick ogni 10 ms.
void run(LiveRx& live, const Stream& s, uint32_t gapMs, const std::vector<size_t>& drop, uint32_t& clock, uint32_t extraMs) {
  size_t next = 0;
  const uint32_t end = static_cast<uint32_t>(s.pkts.size()) * gapMs + extraMs;
  for (clock = 0; clock <= end; clock += 10) {
    while (next < s.pkts.size() && clock >= next * gapMs) {
      bool dropped = false;
      for (size_t d : drop) dropped = dropped || d == next;
      if (!dropped) live.feed(cap(s.pkts[next]), clock);
      next++;
    }
    live.tick(clock);
  }
}

void testStandard() {
  const Stream s = makeStream(100, false, true);  // 100 frame = 4 s di voce
  Out out;
  uint32_t clock = 0;
  out.clock = &clock;
  LiveRx live(out);
  run(live, s, 350, {}, clock, 8000);
  // Tutti i frame, uguali e in ordine.
  CHECK(out.frames.size() == 100);
  bool same = out.frames.size() == s.frames.size();
  for (size_t i = 0; same && i < s.frames.size(); i++) same = out.frames[i] == s.frames[i] && !out.lost[i];
  CHECK(same);
  // A ritmo reale: un frame ogni 40 ms (stessa cadenza della scheda), e il primo non prima del pre-buffer di 1 s
  // (25 frame) dall'arrivo del primo pacchetto con audio.
  // I primi frame escono in blocco (cuscinetto di 240 ms = 6 frame), poi uno ogni 40 ms.
  for (size_t i = 8; i < out.at.size(); i++) CHECK(out.at[i] - out.at[i - 1] >= 30 && out.at[i] - out.at[i - 1] <= 50);
  CHECK(!out.at.empty() && out.at.front() >= 350);
  // Informazioni per l'operatore.
  CHECK(out.has("inizio trasmissione"));
  CHECK(out.has("da IU5HJU"));
  CHECK(out.has("posizione 43.769") && out.has(", 11.255"));  // arrotondata al passo del formato (circa 1-2 m)
  CHECK(out.has("fine: da IU5HJU, 100 frame (4.0 s), persi 0, pacchetti recuperati 0"));
  CHECK(!live.receiving());
  // La voce senza lunghezza dichiarata non ha l'avanzamento.
  CHECK(!out.has("avanzamento"));
}

void testLead() {
  const Stream s = makeStream(100, false, false);
  // Con il cuscinetto (240 ms) i primi 7 frame escono insieme; senza (0) uno ogni 40 ms sin dal primo.
  Out a, b;
  uint32_t ca = 0, cb = 0;
  a.clock = &ca;
  b.clock = &cb;
  LiveRx withLead(a, false, false, 240), noLead(b, false, false, 0);
  run(withLead, s, 350, {}, ca, 8000);
  run(noLead, s, 350, {}, cb, 8000);
  CHECK(a.frames.size() == 100 && b.frames.size() == 100);
  CHECK(a.at.size() == 100 && b.at.size() == 100);
  CHECK(a.at[6] - a.at[0] <= 20);                       // blocco iniziale: il primo frame e 6 di cuscinetto
  CHECK(a.at[7] - a.at[6] >= 30 && a.at[7] - a.at[6] <= 50);  // poi il ritmo reale
  CHECK(b.at[5] - b.at[0] >= 180);                      // senza cuscinetto: 5 intervalli da 40 ms
  // Il cuscinetto anticipa l'audio, non lo ritarda; la durata totale resta quella del messaggio (meno l'anticipo).
  CHECK(a.at.front() == b.at.front());
  CHECK(a.at.back() + 200 <= b.at.back() + 40 && a.at.back() + 280 >= b.at.back());
  // Stesso contenuto, nello stesso ordine.
  CHECK(a.frames == b.frames);
}

void testRecovery() {
  const Stream s = makeStream(100, false, false);
  // Un pacchetto dati perso: lo ricostruisce la parita', i frame sono tutti giusti.
  {
    Out out;
    uint32_t clock = 0;
    LiveRx live(out);
    run(live, s, 350, {2}, clock, 8000);
    CHECK(out.frames.size() == 100);
    bool same = out.frames.size() == s.frames.size();
    for (size_t i = 0; same && i < s.frames.size(); i++) same = out.frames[i] == s.frames[i];
    CHECK(same);
    CHECK(out.has("pacchetti recuperati 1"));
  }
  // Due pacchetti dello stesso gruppo persi: 24 frame persi, in uscita a zero (come in lcr_decode).
  {
    Out out;
    uint32_t clock = 0;
    LiveRx live(out);
    run(live, s, 350, {2, 3}, clock, 8000);
    CHECK(out.frames.size() == 100);
    size_t lost = 0;
    for (size_t i = 0; i < out.frames.size(); i++) {
      if (out.lost[i]) {
        lost++;
        for (uint8_t b : out.frames[i]) CHECK(b == 0);
      }
    }
    CHECK(lost == 24);
    CHECK(out.has("persi 24"));
  }
  // CRC errato: scartato, e come se il pacchetto si fosse perso.
  {
    Out out;
    LiveRx live(out);
    live.feed(cap(s.pkts[0], false), 0);
    CHECK(!live.receiving() && out.has("CRC errato"));
  }
}

void testMessage() {
  const Stream s = makeStream(150, true, false);  // 6 s di voce
  Out out;
  uint32_t clock = 0;
  out.clock = &clock;
  LiveRx live(out, true);  // profilo Messaggio
  run(live, s, 3400, {}, clock, 40000);
  CHECK(out.frames.size() == 150);
  bool same = out.frames.size() == s.frames.size();
  for (size_t i = 0; same && i < s.frames.size(); i++) same = out.frames[i] == s.frames[i];
  CHECK(same);
  // La riproduzione parte solo dopo il pacchetto di fine (END, il penultimo: l'ultimo e' la parita' finale), come sulle
  // schede: non durante la ricezione.
  const uint32_t endPacketAt = static_cast<uint32_t>(s.pkts.size() - 2) * 3400;
  CHECK(!out.at.empty() && out.at.front() >= endPacketAt);
  // Avanzamento da 1% a 100%, mai indietro.
  CHECK(out.has("avanzamento 1%") && out.has("avanzamento 100%"));
  int last = 0;
  bool mono = true;
  for (const auto& l : out.lines) {
    int v = 0;
    if (std::sscanf(l.c_str(), "avanzamento %d%%", &v) == 1) {
      mono = mono && v > last;
      last = v;
    }
  }
  CHECK(mono && last == 100);
}

void testTestSession() {
  // Una prova di collegamento non produce audio: solo la riga con l'esito.
  TxSession tx("IU5HJU", 77, true, kModeTest);
  tx.begin();
  Packet p;
  std::vector<std::vector<uint8_t>> pk;
  auto drain = [&]() {
    while (tx.popPacket(p)) {
      uint8_t b[kMaxWireBytes];
      const size_t n = encodePacket(p, b, sizeof(b));
      pk.emplace_back(b, b + n);
    }
  };
  drain();
  for (uint32_t i = 0; i < kTestFrames; i++) {
    uint8_t f[kFrameBytes];
    testFramePattern(77, i, f);
    tx.pushFrame(f);
    drain();
  }
  tx.finish();
  drain();
  Out out;
  LiveRx live(out);
  uint32_t clock = 0;
  size_t next = 0;
  for (clock = 0; clock < pk.size() * 300 + 30000; clock += 10) {
    while (next < pk.size() && clock >= next * 300) live.feed(cap(pk[next++]), clock);
    live.tick(clock);
  }
  CHECK(out.frames.empty());
  CHECK(out.has("inizio prova di collegamento") && out.has("ok 300/300 (100%)"));
}

}  // namespace

int main() {
  testStandard();
  testLead();
  testRecovery();
  testMessage();
  testTestSession();
  TEST_MAIN_END();
}
