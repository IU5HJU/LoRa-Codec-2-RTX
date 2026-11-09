#include "menu.h"
#include "testing.h"

using namespace lcr;
using P = PttButton::Event;
using G = ButtonGesture::Event;
using K = Menu::Outcome::Kind;

static void testGesture() {
  ButtonGesture g(30, 2000);
  // Pressione breve: Short al rilascio.
  CHECK(g.update(true, 100) == G::None);
  CHECK(g.update(true, 140) == G::None);  // Pressed interno
  CHECK(g.down());
  CHECK(g.update(true, 600) == G::None);
  CHECK(g.update(false, 700) == G::None);
  CHECK(g.update(false, 740) == G::Short);
  CHECK(!g.down());

  // Pressione lunga: Long una sola volta, il rilascio non da' Short.
  CHECK(g.update(true, 1000) == G::None);
  CHECK(g.update(true, 1040) == G::None);
  CHECK(g.update(true, 2900) == G::None);
  CHECK(g.update(true, 3040) == G::Long);
  CHECK(g.update(true, 3500) == G::None);
  CHECK(g.update(true, 9000) == G::None);
  CHECK(g.update(false, 9100) == G::None);
  CHECK(g.update(false, 9140) == G::None);

  // Dopo una lunga, una breve torna a funzionare.
  CHECK(g.update(true, 10000) == G::None);
  CHECK(g.update(true, 10040) == G::None);
  CHECK(g.update(false, 10200) == G::None);
  CHECK(g.update(false, 10240) == G::Short);

  // Giro del contatore del tempo a 32 bit.
  ButtonGesture w(30, 2000);
  const uint32_t t0 = 0xFFFFFC00u;
  CHECK(w.update(true, t0) == G::None);
  CHECK(w.update(true, t0 + 40) == G::None);
  CHECK(w.update(true, t0 + 1900) == G::None);
  CHECK(w.update(true, t0 + 2100) == G::Long);
}

static void testMenu() {
  size_t n = 0;
  const MenuItem* items = defaultMenuItems(n);
  CHECK(n == 3);
  Menu m(items, n);
  CHECK(!m.isOpen());

  // Si apre sulla voce in uso.
  CHECK(m.open(1, 0));
  CHECK(m.isOpen() && m.selectedIndex() == 1);
  // Messaggio -> (Qualita' saltata) -> Standard -> Messaggio
  m.next(100);
  CHECK(m.selectedIndex() == 0);
  m.next(200);
  CHECK(m.selectedIndex() == 1);
  // Conferma della stessa voce: niente da cambiare.
  Menu::Outcome o = m.confirm(300);
  CHECK(o.kind == K::Closed && !m.isOpen());

  // Conferma di una voce diversa.
  CHECK(m.open(0, 1000));
  m.next(1100);
  o = m.confirm(1200);
  CHECK(o.kind == K::Selected && o.id == 1);

  // Tempo massimo: 10 s dall'ultimo tasto.
  CHECK(m.open(0, 5000));
  m.next(8000);
  CHECK(!m.tick(17999) && m.isOpen());
  CHECK(m.tick(18000) && !m.isOpen());
  CHECK(!m.tick(30000));

  // Profilo in uso non disponibile: si evidenzia la prima voce scegliibile.
  CHECK(m.open(2, 0));
  CHECK(m.selectedIndex() == 0);
  m.cancel();
  CHECK(!m.isOpen());

  // Nessuna voce disponibile: non si apre.
  static const MenuItem none[] = {{"A", 0, false}};
  Menu e(none, 1);
  CHECK(!e.open(0, 0));
}

static void testFlow() {
  size_t n = 0;
  const MenuItem* items = defaultMenuItems(n);
  Menu m(items, n);
  MenuFlow f(m);
  uint32_t t = 1000;

  // Fuori dal menu il PTT va al nodo, il volume breve alza il volume.
  MenuFlow::Actions a = f.update(P::Pressed, G::None, true, 0, t);
  CHECK(a.pttToNode && !a.volumeUp);
  a = f.update(P::Released, G::None, true, 0, t += 500);
  CHECK(a.pttToNode);
  a = f.update(P::None, G::Short, true, 0, t += 100);
  CHECK(a.volumeUp && !a.pttToNode && !m.isOpen());

  // Volume lungo con la scheda occupata: non apre.
  a = f.update(P::None, G::Long, false, 0, t += 100);
  CHECK(!m.isOpen() && a.beep == MenuBeep::None);

  // Volume lungo a riposo: apre.
  a = f.update(P::None, G::Long, true, 0, t += 100);
  CHECK(m.isOpen() && a.redraw && a.beep == MenuBeep::Open && !a.volumeUp);

  // PTT nel menu: voce successiva, non trasmette, il rilascio non arriva al nodo.
  a = f.update(P::Pressed, G::None, true, 0, t += 500);
  CHECK(!a.pttToNode && a.redraw && a.beep == MenuBeep::Next && m.selectedIndex() == 1);
  a = f.update(P::Released, G::None, true, 0, t += 200);
  CHECK(!a.pttToNode);

  // Conferma con il volume breve: scelto Messaggio (1), salvataggio a carico del chiamante.
  a = f.update(P::None, G::Short, true, 0, t += 200);
  CHECK(a.outcome.kind == K::Selected && a.outcome.id == 1 && a.beep == MenuBeep::Confirm && !m.isOpen());
  CHECK(!a.volumeUp);

  // Uscita con il volume lungo: nessun cambio.
  f.update(P::None, G::Long, true, 1, t += 100);
  CHECK(m.isOpen());
  a = f.update(P::None, G::Long, true, 1, t += 100);
  CHECK(!m.isOpen() && a.outcome.kind == K::None && a.beep == MenuBeep::Cancel);

  // Voce uguale a quella in uso: chiude senza cambiare.
  f.update(P::None, G::Long, true, 0, t += 100);
  a = f.update(P::None, G::Short, true, 0, t += 100);
  CHECK(a.outcome.kind == K::Closed);

  // Dieci secondi senza tasti: chiude da solo, senza cambiare.
  f.update(P::None, G::Long, true, 0, t += 100);
  CHECK(m.isOpen());
  a = f.update(P::None, G::None, true, 0, t += 9000);
  CHECK(m.isOpen() && !a.redraw);
  a = f.update(P::None, G::None, true, 0, t += 1000);
  CHECK(!m.isOpen() && a.redraw && a.beep == MenuBeep::Cancel && a.outcome.kind == K::None);

  // Arriva una ricezione con il menu aperto: si chiude.
  f.update(P::None, G::Long, true, 0, t += 100);
  CHECK(m.isOpen());
  a = f.update(P::None, G::None, false, 0, t += 100);
  CHECK(!m.isOpen() && a.redraw);

  // PTT premuto nel menu e rilasciato dopo la chiusura (volume lungo): il nodo non lo vede.
  f.update(P::None, G::Long, true, 0, t += 100);
  f.update(P::Pressed, G::None, true, 0, t += 100);
  a = f.update(P::None, G::Long, true, 0, t += 100);
  CHECK(!m.isOpen());
  a = f.update(P::Released, G::None, true, 0, t += 100);
  CHECK(!a.pttToNode);
  // E poi il PTT torna normale.
  a = f.update(P::Pressed, G::None, true, 0, t += 100);
  CHECK(a.pttToNode);
}

int main() {
  testGesture();
  testMenu();
  testFlow();
  TEST_MAIN_END();
}
