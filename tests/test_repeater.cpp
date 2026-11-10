// Ripetitore a due schede: decisione per pacchetto, flag "ripetuto", duplicati, modi, durata della sessione e catena completa
// (stazione -> ripetitore -> stazione) con la ricezione di sempre.
#include <cstdlib>
#include <cstring>
#include <vector>

#include "packet.h"
#include "repeater.h"
#include "rx_stream.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;

static std::vector<Packet> makeTurn(uint16_t session, int frames, uint8_t mode = kModeCodec2_1200) {
  std::vector<Packet> v;
  TxSession tx("IU5HJU", session, true, mode);
  auto drain = [&]() {
    Packet p;
    while (tx.popPacket(p)) v.push_back(p);
  };
  tx.begin();
  drain();
  for (int i = 0; i < frames; i++) {
    uint8_t f[kMaxFrameBytes];
    for (size_t k = 0; k < kMaxFrameBytes; k++) f[k] = static_cast<uint8_t>(i * 7 + k * 31 + 1);
    tx.pushFrame(f);
    drain();
  }
  tx.finish();
  drain();
  return v;
}

static void testBasic() {
  Repeater r;
  Packet in = makeTurn(0x1234, 12)[1];
  Packet out;
  CHECK(r.onPacket(in, 1000, out) == Repeater::Verdict::Forward);
  CHECK(out.flags == (in.flags | kFlagRelayed));
  CHECK(out.session == in.session && out.seq == in.seq && out.mode == in.mode);
  CHECK(out.payloadLen == in.payloadLen && memcmp(out.payload, in.payload, in.payloadLen) == 0);
  CHECK(strcmp(out.callsign, in.callsign) == 0);
  // Il pacchetto ripetuto, serializzato e riletto, conserva il flag.
  uint8_t buf[kMaxWireBytes];
  const size_t n = encodePacket(out, buf, sizeof(buf));
  Packet back;
  CHECK(n > 0 && decodePacket(buf, n, back) && (back.flags & kFlagRelayed));
}

static void testLoopAndDuplicates() {
  Repeater r;
  Packet in = makeTurn(7, 12)[1], out, out2;
  CHECK(r.onPacket(in, 0, out) == Repeater::Verdict::Forward);
  // Il pacchetto ripetuto, se lo risente un ripetitore, non si ripete.
  CHECK(r.onPacket(out, 100, out2) == Repeater::Verdict::AlreadyRelayed);
  // Lo stesso pacchetto arrivato due volte: una sola copia.
  CHECK(r.onPacket(in, 200, out2) == Repeater::Verdict::Duplicate);
  // Dopo la finestra e' di nuovo valido (nuova trasmissione con la stessa sessione e sequenza).
  CHECK(r.onPacket(in, 20000, out2) == Repeater::Verdict::Forward);
  CHECK(r.stats().forwarded == 2 && r.stats().duplicate == 1 && r.stats().alreadyRelayed == 1);
}

static void testModeFilter() {
  Repeater r;
  Packet out;
  CHECK(r.onPacket(makeTurn(1, 12, kModeCodec2_3200)[1], 0, out) == Repeater::Verdict::ModeNotRepeated);  // solo il 1200 di base
  Packet test = makeTurn(2, 12)[1];
  test.mode = kModeTest;
  CHECK(r.onPacket(test, 0, out) == Repeater::Verdict::ModeNotRepeated);
  r.setModeMask((1u << kModeCodec2_1200) | (1u << kModeTest));
  CHECK(r.onPacket(test, 10, out) == Repeater::Verdict::Forward);
  // Spento: niente.
  r.setEnabled(false);
  CHECK(r.onPacket(makeTurn(3, 12)[1], 20, out) == Repeater::Verdict::Disabled);
  r.setEnabled(true);
  CHECK(r.onPacket(makeTurn(4, 12)[1], 30, out) == Repeater::Verdict::Forward);
}

static void testSessionLimit() {
  Repeater r;
  r.setMaxSessionMs(1000);
  Packet out;
  Packet a = makeTurn(9, 48)[1];
  Packet b = a;
  b.seq = a.seq + 1;
  Packet c = a;
  c.seq = a.seq + 2;
  CHECK(r.onPacket(a, 0, out) == Repeater::Verdict::Forward);
  CHECK(r.onPacket(b, 900, out) == Repeater::Verdict::Forward);
  CHECK(r.onPacket(c, 1500, out) == Repeater::Verdict::SessionTooLong);  // oltre il limite della stessa sessione
  Packet other = makeTurn(10, 12)[1];
  CHECK(r.onPacket(other, 1600, out) == Repeater::Verdict::Forward);     // un'altra sessione riparte
}

// Catena completa: la voce di una stazione attraversa il ripetitore, con perdite sul primo tratto recuperate dal
// ricevitore finale con la parita'. Il ricevitore finale e' quello di sempre.
static void testChain() {
  const int frames = 120;
  const auto turn = makeTurn(0xBEEF, frames);
  Repeater rep;
  RxStream rx;
  size_t played = 0, lost = 0, got = 0;
  uint32_t t = 0;
  size_t idx = 0;
  const size_t dropped = 3;  // si perde un pacchetto dati (recuperabile con la parita')
  for (const Packet& p : turn) {
    idx++;
    t += 480;
    if (idx == dropped) continue;  // perso sul tratto stazione-ripetitore: il ripetitore non lo vede
    Packet out;
    if (rep.onPacket(p, t, out) == Repeater::Verdict::Forward) {
      CHECK(rx.onPacket(out, t));
      got++;
    }
  }
  // Riproduzione di tutto: un frame ogni 40 ms di tempo simulato.
  for (int i = 0; i < 4000; i++) {
    t += 40;
    const RxStream::Out o = rx.nextFrame(t);
    if (o.kind == RxStream::Kind::Frame) {
      played++;
      // contenuto originale
      uint8_t exp[kMaxFrameBytes];
      const int fr = static_cast<int>(played - 1);
      for (size_t k = 0; k < kMaxFrameBytes; k++) exp[k] = static_cast<uint8_t>(fr * 7 + k * 31 + 1);
      CHECK(memcmp(o.frame, exp, kFrameBytes) == 0);
    } else if (o.kind == RxStream::Kind::Lost) {
      lost++;
    } else if (o.kind == RxStream::Kind::End) {
      break;
    }
  }
  CHECK(got == turn.size() - 1);
  CHECK(played == static_cast<size_t>(frames));  // il pacchetto perso e' stato ricostruito dalla parita'
  CHECK(lost == 0);
}

int main() {
  testBasic();
  testLoopAndDuplicates();
  testModeFilter();
  testSessionLimit();
  testChain();
  TEST_MAIN_END();
}
