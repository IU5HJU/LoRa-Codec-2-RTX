// Scelta di una voce da un breve elenco con i due tasti (PTT = avanti, volume breve = conferma, volume lungo o 10 s senza tasti =
// esci). Serve alla voce "MeshCom" del menu: prima il testo (con "solo ascolto" in fondo), poi il destinatario.
// Il PTT non trasmette mai finche' e' aperto. Indipendente dalla scheda e testato sul PC (tests/test_choice_picker.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "button_gesture.h"
#include "menu.h"
#include "ptt_button.h"

namespace lcr {

class ChoicePicker {
 public:
  static constexpr size_t kMaxItems = 8;
  struct Actions {
    bool redraw = false;
    MenuBeep beep = MenuBeep::None;
    bool chosen = false;      // confermata la voce index
    size_t index = 0;
    bool swallowPtt = false;  // l'evento del PTT di questo giro e' stato usato qui
  };

  bool isOpen() const { return open_; }
  // Apre sulla prima voce. items: puntatori a stringhe che devono restare valide finche' e' aperto. false se n == 0.
  bool open(const char* const* items, size_t n, uint32_t nowMs);
  Actions update(PttButton::Event ptt, ButtonGesture::Event vol, bool nodeIdle, uint32_t nowMs);
  void cancel() { open_ = false; }

  size_t index() const { return index_; }
  size_t count() const { return n_; }
  const char* text() const { return open_ || n_ ? items_[index_] : ""; }

 private:
  const char* items_[kMaxItems] = {nullptr};
  size_t n_ = 0;
  size_t index_ = 0;
  bool open_ = false;
  uint32_t lastMs_ = 0;
  bool pttSwallowed_ = false;
};

}  // namespace lcr
