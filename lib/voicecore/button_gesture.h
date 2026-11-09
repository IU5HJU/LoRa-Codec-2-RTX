// Gesti di un pulsante: pressione breve e pressione lunga. Indipendente dalla scheda: si passa lo stato grezzo del pin
// (gia' convertito in "premuto" = true) e il tempo, come per PttButton.
//
//   Short  il pulsante e' stato rilasciato prima di longMs
//   Long   il pulsante e' ancora premuto da longMs: l'evento arriva UNA volta, mentre e' premuto, e il rilascio che segue
//          non produce nessun Short
#pragma once

#include <cstdint>

#include "ptt_button.h"

namespace lcr {

class ButtonGesture {
 public:
  enum class Event { None, Short, Long };

  explicit ButtonGesture(uint32_t debounceMs = 30, uint32_t longMs = 2000) : button_(debounceMs), longMs_(longMs) {}

  // Da chiamare spesso.
  Event update(bool rawPressed, uint32_t nowMs) {
    switch (button_.update(rawPressed, nowMs)) {
      case PttButton::Event::Pressed:
        pressedAtMs_ = nowMs;
        longFired_ = false;
        return Event::None;
      case PttButton::Event::Released:
        return longFired_ ? Event::None : Event::Short;
      case PttButton::Event::None: break;
    }
    if (button_.pressed() && !longFired_ && static_cast<int32_t>(nowMs - pressedAtMs_) >= static_cast<int32_t>(longMs_)) {
      longFired_ = true;
      return Event::Long;
    }
    return Event::None;
  }

  bool down() const { return button_.pressed(); }

 private:
  PttButton button_;
  uint32_t longMs_;
  uint32_t pressedAtMs_ = 0;
  bool longFired_ = false;
};

}  // namespace lcr
