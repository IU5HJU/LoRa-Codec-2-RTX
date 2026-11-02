// Antirimbalzo del pulsante PTT. Indipendente dalla scheda: si passa lo stato
// grezzo del pin (gia' convertito in "premuto" = true) e il tempo.
#pragma once

#include <cstdint>

namespace lcr {

class PttButton {
 public:
  enum class Event { None, Pressed, Released };

  explicit PttButton(uint32_t debounceMs = 30) : debounceMs_(debounceMs) {}

  // Da chiamare spesso. Ritorna Pressed/Released una sola volta per ogni
  // cambio di stato che resta stabile per almeno debounceMs.
  Event update(bool rawPressed, uint32_t nowMs) {
    if (rawPressed != lastRaw_) {
      lastRaw_ = rawPressed;
      changedAt_ = nowMs;
    }
    if (lastRaw_ != stable_ && nowMs - changedAt_ >= debounceMs_) {
      stable_ = lastRaw_;
      return stable_ ? Event::Pressed : Event::Released;
    }
    return Event::None;
  }

  bool pressed() const { return stable_; }

 private:
  uint32_t debounceMs_;
  bool lastRaw_ = false;
  bool stable_ = false;
  uint32_t changedAt_ = 0;
};

}  // namespace lcr
