#include "ptt_button.h"
#include "testing.h"

using namespace lcr;
using E = PttButton::Event;

int main() {
  PttButton b(30);
  CHECK(b.update(false, 0) == E::None && !b.pressed());

  // Pressione con rimbalzi: un solo evento Pressed, dopo 30 ms di stabilita'.
  CHECK(b.update(true, 100) == E::None);
  CHECK(b.update(false, 105) == E::None);
  CHECK(b.update(true, 110) == E::None);
  CHECK(b.update(true, 130) == E::None);  // solo 20 ms dall'ultimo cambio
  CHECK(b.update(true, 140) == E::Pressed);
  CHECK(b.pressed());
  CHECK(b.update(true, 150) == E::None);
  CHECK(b.update(true, 5000) == E::None);

  // Un impulso piu' breve dell'antirimbalzo e' ignorato.
  CHECK(b.update(false, 6000) == E::None);
  CHECK(b.update(true, 6010) == E::None);
  CHECK(b.update(true, 6100) == E::None && b.pressed());

  // Rilascio.
  CHECK(b.update(false, 7000) == E::None);
  CHECK(b.update(false, 7030) == E::Released);
  CHECK(!b.pressed());
  CHECK(b.update(false, 7100) == E::None);

  // Disturbo breve a pulsante rilasciato: nessun evento.
  CHECK(b.update(true, 8000) == E::None);
  CHECK(b.update(false, 8010) == E::None);
  CHECK(b.update(false, 8100) == E::None);

  TEST_MAIN_END();
}
