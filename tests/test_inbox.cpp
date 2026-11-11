#include <cstring>

#include "inbox.h"
#include "testing.h"

using namespace lcr;

int main() {
  Inbox b;
  CHECK(!b.has());
  uint8_t f[kFrameBytes], o[kFrameBytes];

  b.beginVoice();
  for (int i = 0; i < 5; i++) {
    memset(f, i + 1, sizeof(f));
    if (i == 2) CHECK(b.pushLost()); else CHECK(b.pushFrame(f));
  }
  CHECK(!b.has());  // non e' ancora un messaggio finche' non si conferma
  CHECK(b.commitVoice("IU5HJU"));
  CHECK(b.kind() == Inbox::Kind::Voice && b.frames() == 5 && strcmp(b.callsign(), "IU5HJU") == 0);
  CHECK(b.frameAt(0, o) && o[0] == 1);
  CHECK(!b.frameAt(2, o));  // perso
  CHECK(b.frameAt(4, o) && o[5] == 5);
  CHECK(!b.frameAt(5, o));

  // un messaggio nuovo cancella il vecchio
  CHECK(b.setText("IW5ABC", "ciao", 4));
  CHECK(b.kind() == Inbox::Kind::Text && strcmp(b.text(), "ciao") == 0 && b.textLen() == 4 && !b.frameAt(0, o));
  // testo non valido: casella invariata
  CHECK(!b.setText("IW5ABC", "a\nb", 3));
  CHECK(strcmp(b.text(), "ciao") == 0);
  CHECK(!b.setText("IW5ABC", "", 0));

  b.beginVoice();
  CHECK(!b.commitVoice("X1X"));  // nessun frame
  CHECK(!b.has());

  // capienza: 30 s
  b.beginVoice();
  for (size_t i = 0; i < kMaxTurnFrames; i++) CHECK(b.pushFrame(f));
  CHECK(!b.pushFrame(f) && !b.pushLost());
  CHECK(b.commitVoice("IU5HJU") && b.frames() == kMaxTurnFrames);
  b.clear();
  CHECK(!b.has());
  TEST_MAIN_END();
}
