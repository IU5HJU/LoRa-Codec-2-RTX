#include "text_picker.h"

namespace lcr {

size_t TextPicker::usedCount() const {
  size_t n = 0;
  if (texts_) {
    for (size_t i = 0; i < kTextSlots; i++) {
      if (texts_[i][0]) n++;
    }
  }
  return n;
}

size_t TextPicker::ordinal() const {
  size_t n = 0;
  if (texts_) {
    for (size_t i = 0; i <= slot_ && i < kTextSlots; i++) {
      if (texts_[i][0]) n++;
    }
  }
  return n;
}

bool TextPicker::open(const Settings& s, uint32_t nowMs) {
  texts_ = s.texts;
  open_ = false;
  for (size_t i = 0; i < kTextSlots; i++) {
    if (texts_[i][0]) {
      slot_ = i;
      open_ = true;
      lastMs_ = nowMs;
      pttSwallowed_ = false;
      return true;
    }
  }
  texts_ = nullptr;
  return false;
}

void TextPicker::next() {
  for (size_t step = 1; step <= kTextSlots; step++) {
    const size_t i = (slot_ + step) % kTextSlots;
    if (texts_[i][0]) {
      slot_ = i;
      return;
    }
  }
}

TextPicker::Actions TextPicker::update(PttButton::Event ptt, ButtonGesture::Event vol, bool nodeIdle, uint32_t nowMs) {
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
    next();
    pttSwallowed_ = true;
    a.swallowPtt = true;
    a.redraw = true;
    a.beep = MenuBeep::Next;
    lastMs_ = nowMs;
  }
  if (vol == ButtonGesture::Event::Short) {
    open_ = false;
    a.send = true;
    a.slot = static_cast<int>(slot_);
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
