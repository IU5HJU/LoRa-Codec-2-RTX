#include <cstring>

#include "testing.h"
#include "text_picker.h"

using namespace lcr;
using P = PttButton::Event;
using G = ButtonGesture::Event;

int main() {
  Settings s;  // cinque testi di esempio
  TextPicker t;
  CHECK(!t.isOpen());
  CHECK(t.open(s, 0) && t.isOpen() && t.slot() == 0 && strcmp(t.text(), "%C QRV su VoLoRa modo %M %F (Inviato da dispositivo VoLoRa)") == 0);
  CHECK(t.usedCount() == 5 && t.ordinal() == 1);

  // PTT: avanti, con giro; il rilascio non va al nodo
  auto a = t.update(P::Pressed, G::None, true, 100);
  CHECK(t.slot() == 1 && a.redraw && a.beep == MenuBeep::Next && a.swallowPtt && !a.send);
  a = t.update(P::Released, G::None, true, 200);
  CHECK(a.swallowPtt);
  for (int i = 0; i < 4; i++) t.update(P::Pressed, G::None, true, 300 + i);
  CHECK(t.slot() == 0);  // giro completo

  // posti vuoti saltati
  s.texts[1][0] = 0;
  s.texts[2][0] = 0;
  CHECK(t.open(s, 1000) && t.usedCount() == 3);
  t.update(P::Pressed, G::None, true, 1010);
  CHECK(t.slot() == 3 && t.ordinal() == 2);

  // volume breve invia il testo scelto e chiude
  a = t.update(P::None, G::Short, true, 1100);
  CHECK(a.send && a.slot == 3 && !t.isOpen() && a.beep == MenuBeep::Confirm);

  // volume lungo esce senza inviare
  CHECK(t.open(s, 2000));
  a = t.update(P::None, G::Long, true, 2100);
  CHECK(!a.send && !t.isOpen() && a.beep == MenuBeep::Cancel);

  // nessun tasto per 10 s: esce; scheda occupata: esce
  CHECK(t.open(s, 3000));
  a = t.update(P::None, G::None, true, 3000 + kMenuTimeoutMs);
  CHECK(!a.send && !t.isOpen());
  CHECK(t.open(s, 4000));
  a = t.update(P::None, G::None, false, 4100);
  CHECK(!a.send && !t.isOpen());
  // un tasto rinnova il tempo
  CHECK(t.open(s, 5000));
  t.update(P::Pressed, G::None, true, 5000 + kMenuTimeoutMs - 100);
  a = t.update(P::None, G::None, true, 5000 + kMenuTimeoutMs + 100);
  CHECK(t.isOpen());

  // tutti vuoti: non si apre
  Settings none;
  for (size_t i = 0; i < kTextSlots; i++) none.texts[i][0] = 0;
  CHECK(!t.open(none, 0) && !t.isOpen());
  TEST_MAIN_END();
}
