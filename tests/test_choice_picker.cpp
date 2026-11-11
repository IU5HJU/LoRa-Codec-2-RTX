#include <cstring>

#include "choice_picker.h"
#include "testing.h"

using namespace lcr;
using P = PttButton::Event;
using G = ButtonGesture::Event;

int main() {
  ChoicePicker c;
  const char* items[] = {"22251", "222", "*", "IU5ABC-9"};
  CHECK(!c.isOpen() && !c.open(nullptr, 3, 0) && !c.open(items, 0, 0) && !c.isOpen());
  CHECK(c.open(items, 4, 0) && c.isOpen() && c.index() == 0 && strcmp(c.text(), "22251") == 0 && c.count() == 4);
  // PTT: avanti con giro; il rilascio non va al nodo
  auto a = c.update(P::Pressed, G::None, true, 100);
  CHECK(c.index() == 1 && a.redraw && a.beep == MenuBeep::Next && a.swallowPtt && !a.chosen);
  a = c.update(P::Released, G::None, true, 200);
  CHECK(a.swallowPtt);
  for (int i = 0; i < 3; i++) c.update(P::Pressed, G::None, true, 300 + i);
  CHECK(c.index() == 0);
  c.update(P::Pressed, G::None, true, 400);
  c.update(P::Pressed, G::None, true, 401);
  CHECK(strcmp(c.text(), "*") == 0);
  // volume breve: conferma la voce corrente e chiude
  a = c.update(P::None, G::Short, true, 500);
  CHECK(a.chosen && a.index == 2 && !c.isOpen() && a.beep == MenuBeep::Confirm);
  // volume lungo: esce senza scegliere
  CHECK(c.open(items, 4, 1000));
  a = c.update(P::None, G::Long, true, 1100);
  CHECK(!a.chosen && !c.isOpen() && a.beep == MenuBeep::Cancel);
  // 10 s senza tasti, scheda occupata: esce; un tasto rinnova il tempo
  CHECK(c.open(items, 4, 2000));
  a = c.update(P::None, G::None, true, 2000 + kMenuTimeoutMs);
  CHECK(!a.chosen && !c.isOpen());
  CHECK(c.open(items, 4, 3000));
  a = c.update(P::None, G::None, false, 3100);
  CHECK(!a.chosen && !c.isOpen());
  CHECK(c.open(items, 4, 4000));
  c.update(P::Pressed, G::None, true, 4000 + kMenuTimeoutMs - 100);
  a = c.update(P::None, G::None, true, 4000 + kMenuTimeoutMs + 100);
  CHECK(c.isOpen());
  // una sola voce: si puo' confermare subito
  const char* one[] = {"solo ascolto"};
  CHECK(c.open(one, 1, 0));
  c.update(P::Pressed, G::None, true, 10);
  CHECK(c.index() == 0);
  a = c.update(P::None, G::Short, true, 20);
  CHECK(a.chosen && a.index == 0);
  TEST_MAIN_END();
}
