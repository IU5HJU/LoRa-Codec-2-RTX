#include "choice_picker.h"

namespace lcr {

bool ChoicePicker::open(const char* const* items, size_t n, uint32_t nowMs) {
  open_ = false;
  if (!items || n == 0 || n > kMaxItems) return false;
  for (size_t i = 0; i < n; i++) items_[i] = items[i] ? items[i] : "";
  n_ = n;
  index_ = 0;
  open_ = true;
  lastMs_ = nowMs;
  pttSwallowed_ = false;
  return true;
}

ChoicePicker::Actions ChoicePicker::update(PttButton::Event ptt, ButtonGesture::Event vol, bool nodeIdle, uint32_t nowMs) {
  Actions a;
  if (!open_) return a;
  if (ptt == PttButton::Event::Released && pttSwallowed_) {
    pttSwallowed_ = false;
    a.swallowPtt = true;
  }
  if (!nodeIdle || static_cast<int32_t>(nowMs - lastMs_) >= static_cast<int32_t>(kMenuTimeoutMs)) {
    open_ = false;
    a.redraw = true;
    a.beep = MenuBeep::Cancel;
    return a;
  }
  if (ptt == PttButton::Event::Pressed) {
    index_ = (index_ + 1) % n_;
    pttSwallowed_ = true;
    a.swallowPtt = true;
    a.redraw = true;
    a.beep = MenuBeep::Next;
    lastMs_ = nowMs;
  }
  if (vol == ButtonGesture::Event::Short) {
    open_ = false;
    a.chosen = true;
    a.index = index_;
    a.redraw = true;
    a.beep = MenuBeep::Confirm;
  } else if (vol == ButtonGesture::Event::Long) {
    open_ = false;
    a.redraw = true;
    a.beep = MenuBeep::Cancel;
  }
  return a;
}

}  // namespace lcr
