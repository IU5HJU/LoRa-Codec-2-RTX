// Scelta del testo da inviare (voce "Invia testo" del menu), con i due soliti tasti:
//   PTT, pressione breve        testo successivo (i posti vuoti si saltano, con giro dall'ultimo al primo)
//   volume, pressione breve     invia il testo scelto
//   volume, pressione lunga     esce senza inviare
//   10 s senza toccare niente   esce senza inviare
// Nella scelta il PTT non trasmette mai. Indipendente dalla scheda e testato sul PC (tests/test_text_picker.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "button_gesture.h"
#include "menu.h"
#include "ptt_button.h"
#include "settings.h"

namespace lcr {

class TextPicker {
 public:
  struct Actions {
    bool redraw = false;
    MenuBeep beep = MenuBeep::None;
    bool send = false;  // confermato: inviare text()
    int slot = -1;      // posto del testo da inviare (0-based) quando send e' true
    bool swallowPtt = false;  // l'evento del PTT di questo giro e' stato usato qui (non va al nodo)
  };

  bool isOpen() const { return open_; }
  // Apre con il primo testo non vuoto. false (e non si apre) se tutti i posti sono vuoti.
  bool open(const Settings& s, uint32_t nowMs);
  // PTT = avanti, volume breve = invia, volume lungo = esci; chiude da solo dopo kMenuTimeoutMs o se il nodo non e' a riposo.
  Actions update(PttButton::Event ptt, ButtonGesture::Event vol, bool nodeIdle, uint32_t nowMs);

  size_t slot() const { return slot_; }
  const char* text() const { return texts_ ? texts_[slot_] : ""; }
  // Posizione tra i testi non vuoti (1-based) e loro numero, per "2/4" sul display.
  size_t ordinal() const;
  size_t usedCount() const;

 private:
  void next();
  const char (*texts_)[kMaxTextChars + 1] = nullptr;
  bool open_ = false;
  size_t slot_ = 0;
  uint32_t lastMs_ = 0;
  bool pttSwallowed_ = false;
};

}  // namespace lcr
