#include "ui.h"

#include <Wire.h>

#include "board_pins.h"
#include "user_config.h"

bool UiNotifier::begin() {
  Wire.begin(pins::kOledSda, pins::kOledScl);
  ok_ = display_.begin(SSD1306_SWITCHCAPVCC, LCR_OLED_ADDRESS);
  if (ok_) {
    display_.setTextColor(SSD1306_WHITE);
    display_.clearDisplay();
    display_.display();
  }
  return ok_;
}

void UiNotifier::showMessage(const char* line1, const char* line2) {
  if (!ok_) return;
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.println(LCR_CALLSIGN);
  display_.setTextSize(2);
  display_.setCursor(0, 20);
  display_.println(line1);
  if (line2) {
    display_.setTextSize(1);
    display_.setCursor(0, 52);
    display_.println(line2);
  }
  display_.display();
}

void UiNotifier::txStarted() {
  txStartMs_ = millis();
  limitHit_ = false;
  haveLast_ = false;
  dirty_ = true;
}

void UiNotifier::txEnded(bool turnLimitReached) {
  limitHit_ = turnLimitReached;
  if (turnLimitReached) {
    notice_ = "LIMITE 30 s";
    noticeUntilMs_ = millis() + 2000;
  }
  dirty_ = true;
  audio_.beep(1000, 50);  // "roger": fine della trasmissione
}

void UiNotifier::pttDenied(lcr::DenyReason r) {
  switch (r) {
    case lcr::DenyReason::Receiving: notice_ = "STAI RICEVENDO"; break;
    case lcr::DenyReason::ChannelBusy: notice_ = "CANALE OCCUPATO"; break;
    case lcr::DenyReason::Transmitting: notice_ = "TX IN CORSO"; break;
    case lcr::DenyReason::NoCallsign: notice_ = "IMPOSTA NOMINATIVO"; break;
  }
  noticeUntilMs_ = millis() + 1500;
  dirty_ = true;
  audio_.beep(300, 150);
}

void UiNotifier::rxStarted() {
  rxCall_[0] = 0;
  haveLast_ = false;
  dirty_ = true;
}

void UiNotifier::rxCallsign(const char* callsign) {
  strncpy(rxCall_, callsign, lcr::kMaxCallsign);
  rxCall_[lcr::kMaxCallsign] = 0;
  dirty_ = true;
}

void UiNotifier::rxEnded(const char* callsign, uint32_t lostFrames, uint32_t recoveredPackets) {
  if (callsign && callsign[0]) {
    strncpy(rxCall_, callsign, lcr::kMaxCallsign);
    rxCall_[lcr::kMaxCallsign] = 0;
  }
  lastLost_ = lostFrames;
  lastRecovered_ = recoveredPackets;
  haveLast_ = true;
  // Riga per il registro delle prove (docs/prove.md): valori dell'ultimo pacchetto ricevuto.
  Serial.printf("RX fine: %s RSSI %d dBm SNR %.1f dB, frame persi %lu, pacchetti recuperati %lu\n",
                rxCall_[0] ? rxCall_ : "-", rssi_, static_cast<double>(snr_), static_cast<unsigned long>(lostFrames),
                static_cast<unsigned long>(recoveredPackets));
  dirty_ = true;
  audio_.beep(800, 40);  // fine della trasmissione ricevuta (OVER)
}

void UiNotifier::loop(uint32_t nowMs, lcr::VoiceNode::State state, int volumeStep, int rssiDbm, float snrDb) {
  if (state != state_ || volumeStep != volume_) dirty_ = true;
  state_ = state;
  volume_ = volumeStep;
  rssi_ = rssiDbm;
  snr_ = snrDb;
  if (notice_ && nowMs > noticeUntilMs_) {
    notice_ = nullptr;
    dirty_ = true;
  }
  const bool periodic = state != lcr::VoiceNode::State::Idle && nowMs - lastDrawMs_ >= 250;
  if (!dirty_ && !periodic) return;
  if (nowMs - lastDrawMs_ < 100) return;  // l'I2C del display costa ~25 ms: non piu' di 10 Hz
  draw(nowMs);
  lastDrawMs_ = nowMs;
  dirty_ = false;
}

void UiNotifier::draw(uint32_t nowMs) {
  if (!ok_) return;
  display_.clearDisplay();

  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.printf("%s  %.3f MHz", LCR_CALLSIGN, static_cast<double>(LCR_FREQUENCY_MHZ));
  display_.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  display_.setTextSize(2);
  display_.setCursor(0, 16);
  switch (state_) {
    case lcr::VoiceNode::State::Idle: display_.print("ASCOLTO"); break;
    case lcr::VoiceNode::State::Receiving: display_.printf("RX %s", rxCall_); break;
    case lcr::VoiceNode::State::Transmitting: display_.printf("TX %s", LCR_CALLSIGN); break;
  }

  display_.setTextSize(1);
  display_.setCursor(0, 38);
  if (notice_) {
    display_.print(notice_);
  } else if (state_ == lcr::VoiceNode::State::Transmitting) {
    display_.printf("Trasmissione %lu/30 s", static_cast<unsigned long>((nowMs - txStartMs_) / 1000));
  } else if (state_ == lcr::VoiceNode::State::Receiving) {
    display_.printf("RSSI %d  SNR %.1f", rssi_, static_cast<double>(snr_));
  } else if (haveLast_) {
    display_.printf("Ultimo %s", rxCall_[0] ? rxCall_ : "-");
    display_.setCursor(0, 48);
    display_.printf("persi %lu  recup. %lu", static_cast<unsigned long>(lastLost_),
                    static_cast<unsigned long>(lastRecovered_));
  } else {
    display_.print("Premi PTT per parlare");
  }

  display_.setCursor(0, 56);
  display_.printf("Vol %d/10", volume_);
  display_.display();
}
