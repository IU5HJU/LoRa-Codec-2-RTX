#include "ui.h"

#include <Wire.h>

#include <cmath>

#include "battery_info.h"

#include "board_pins.h"
#include "user_config.h"

namespace {
// Riga grande (testo 2: 12 px per carattere, 128 px = 10 caratteri). Se il testo e' piu' lungo (per esempio "RX IU5HJU/P")
// scende al testo piccolo invece di andare a capo sulla riga sotto.
void bigText(Adafruit_SSD1306& d, int y, const char* text) {
  const bool fits = strlen(text) <= 10;
  d.setTextSize(fits ? 2 : 1);
  d.setCursor(0, fits ? y : y + 4);
  d.print(text);
}
}  // namespace

bool UiNotifier::begin() {
  Wire.begin(pins::kOledSda, pins::kOledScl);
  // Un display montato a mano puo' stare a 0x3C o a 0x3D: si prova prima quello di user_config.h, poi l'altro.
  const uint8_t addrs[2] = {static_cast<uint8_t>(LCR_OLED_ADDRESS), static_cast<uint8_t>(LCR_OLED_ADDRESS == 0x3C ? 0x3D : 0x3C)};
  for (uint8_t a : addrs) {
    ok_ = display_.begin(SSD1306_SWITCHCAPVCC, a);
    if (ok_) {
      Serial.printf("[avvio] display OLED trovato all'indirizzo 0x%02X\n", a);
      break;
    }
  }
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
  display_.println(settings_.callsign);
  bigText(display_, 20, line1);
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
    snprintf(noticeBuf_, sizeof(noticeBuf_), "LIMITE %lu s", static_cast<unsigned long>(maxTurnSec_));
    notice_ = noticeBuf_;
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
  playing_ = false;
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

void UiNotifier::linkTestReport(bool fromRemote, const char* callsign, const lcr::TestReport& r) {
  testFromRemote_ = fromRemote;
  strncpy(testCall_, callsign ? callsign : "", lcr::kMaxCallsign);
  testCall_[lcr::kMaxCallsign] = 0;
  testRep_ = r;
  testShown_ = true;
  testUntilMs_ = millis() + 15000;
  // Riga per il registro delle prove (docs/prove.md).
  Serial.printf("PROVA %s %s: ok %u/%u (%d%%)%s, danneggiati %u, persi %u, pacchetti recuperati %u, RSSI %d dBm, SNR %s%d dB\n",
                fromRemote ? "risposta da" : "ricevuta da", testCall_[0] ? testCall_ : "-", r.framesOk, r.framesOut,
                r.percentOk(), r.complete() ? "" : " PARZIALE (prova interrotta o coda persa)", r.framesBad, r.framesLost,
                r.recovered, r.rssiDbm, r.snrValid ? "" : "n.d. ", r.snrValid ? r.snrDb : 0);
  dirty_ = true;
  if (r.clean()) {
    audio_.beep(1000, 60);
    audio_.beep(1400, 60);
  } else {
    audio_.beep(300, 200);
  }
}

void UiNotifier::testModeChanged(bool on) {
  notice_ = on ? "PROVA ATTIVA" : "PROVA SPENTA";
  noticeUntilMs_ = millis() + 1500;
  testMode_ = on;
  dirty_ = true;
  if (on) {
    audio_.beep(1000, 60);
    audio_.beep(1400, 60);
  } else {
    audio_.beep(500, 120);
  }
}

void UiNotifier::drawTestReport() {
  const lcr::TestReport& r = testRep_;
  display_.setTextSize(2);
  display_.setCursor(0, 16);
  display_.printf("PROVA %d%%", r.percentOk());
  display_.setTextSize(1);
  display_.setCursor(0, 38);
  if (r.complete()) {
    display_.printf("%s %s", testFromRemote_ ? "Risposta" : "Ricevuta da", testCall_[0] ? testCall_ : "-");
  } else {
    display_.printf("Parz.%u/%u %s", r.framesOut, static_cast<unsigned>(lcr::kTestFrames), testCall_[0] ? testCall_ : "-");
  }
  display_.setCursor(0, 47);
  display_.printf("ok %u pers %u dan %u", r.framesOk, r.framesLost, r.framesBad);  // max 21 caratteri
  display_.setCursor(0, 56);
  // Al massimo 21 caratteri (128 px): "rec 12 -120dBm -15dB" ne usa 20, oltre andrebbe a capo.
  if (r.snrValid) {
    display_.printf("rec %u %ddBm %ddB", r.recovered, r.rssiDbm, r.snrDb);
  } else {
    display_.printf("rec %u %ddBm", r.recovered, r.rssiDbm);
  }
  display_.display();
}

// Menu delle modalita': titolo, una voce per riga (la scelta in negativo, la modalita' in uso con *, le voci non
// disponibili con "presto"), istruzioni in basso. Al massimo 21 caratteri per riga a testo piccolo.
void UiNotifier::drawMenu() {
  display_.setTextSize(1);
  display_.setCursor(0, 13);
  display_.print("MODALITA'");
  for (size_t i = 0; i < menu_->count(); i++) {
    const lcr::MenuItem& it = menu_->item(i);
    const int y = 24 + static_cast<int>(i) * 10;
    const bool sel = i == menu_->selectedIndex();
    if (sel) {
      display_.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
      display_.setTextColor(SSD1306_BLACK);
    }
    display_.setCursor(2, y);
    display_.printf("%c %s%s", it.id == menuCurrentId_ ? '*' : ' ', it.label, it.enabled ? "" : " (presto)");
    display_.setTextColor(SSD1306_WHITE);
  }
  display_.setCursor(0, 56);
  display_.print("PTT=avanti  VOL=ok");
  display_.display();
}

void UiNotifier::loop(uint32_t nowMs, lcr::VoiceNode::State state, int volumeStep, int rssiDbm, float snrDb,
                      float batteryVolts) {
  if (state != state_ || volumeStep != volume_) dirty_ = true;
  state_ = state;
  volume_ = volumeStep;
  rssi_ = rssiDbm;
  snr_ = snrDb;
  battery_ = batteryVolts;
  // Il valore a schermo cambia solo se la misura si sposta di almeno 0,03 V (oppure passa da/a
  // "non disponibile", oppure ogni 10 s): ridisegnare l'OLED costa circa 25 ms.
  const bool availabilityChanged = (battery_ < 0) != (batteryShown_ < 0);
  const bool moved = battery_ >= 0 && batteryShown_ >= 0 && fabsf(battery_ - batteryShown_) >= 0.03f;
  const bool stale = nowMs - batteryShownMs_ >= 10000 && battery_ != batteryShown_;
  if (availabilityChanged || moved || stale) {
    batteryShown_ = battery_;
    batteryShownMs_ = nowMs;
    dirty_ = true;
  }
  if (testShown_ && nowMs > testUntilMs_) {
    testShown_ = false;
    dirty_ = true;
  }
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
  display_.printf("%s  %.3f MHz", settings_.callsign, static_cast<double>(settings_.freqMhz()));
  display_.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  if (menu_ && menu_->isOpen()) {
    drawMenu();
    return;
  }

  if (testShown_ && state_ == lcr::VoiceNode::State::Idle) {
    drawTestReport();
    return;
  }

  char big[24];
  switch (state_) {
    case lcr::VoiceNode::State::Idle: snprintf(big, sizeof(big), "%s", message_ ? "MESSAGGIO" : (testMode_ ? "PROVA ON" : "ASCOLTO")); break;
    case lcr::VoiceNode::State::Receiving: snprintf(big, sizeof(big), "RX %s", rxCall_); break;
    default:
      if (recPhase_ == 1) snprintf(big, sizeof(big), "REGISTRA");
      else if (recPhase_ == 2) snprintf(big, sizeof(big), "ATTESA");
      else if (recPhase_ == 3) snprintf(big, sizeof(big), "INVIO");
      else snprintf(big, sizeof(big), "TX %s", settings_.callsign);
      break;
  }
  bigText(display_, 16, big);

  display_.setTextSize(1);
  display_.setCursor(0, 38);
  if (notice_) {
    display_.print(notice_);
  } else if (state_ == lcr::VoiceNode::State::Transmitting && recPhase_ == 1) {
    display_.printf("Registro %lu/%lu s", static_cast<unsigned long>((nowMs - txStartMs_) / 1000),
                    static_cast<unsigned long>(maxTurnSec_));
  } else if (state_ == lcr::VoiceNode::State::Transmitting && recPhase_ == 2) {
    display_.print("Parte tra 1 secondo");
  } else if (state_ == lcr::VoiceNode::State::Transmitting && recPhase_ == 3) {
    display_.print("Trasmetto messaggio");
  } else if (state_ == lcr::VoiceNode::State::Transmitting) {
    display_.printf(testMode_ ? "Prova %lu/12 s" : "Trasmissione %lu/%lu s",
                    static_cast<unsigned long>((nowMs - txStartMs_) / 1000), static_cast<unsigned long>(maxTurnSec_));
  } else if (state_ == lcr::VoiceNode::State::Receiving && message_) {
    display_.print(playing_ ? "Riproduco messaggio" : "Ricevo il messaggio");
    display_.setCursor(0, 47);
    display_.printf("RSSI %d  SNR %.1f", rssi_, static_cast<double>(snr_));
  } else if (state_ == lcr::VoiceNode::State::Receiving) {
    display_.printf("RSSI %d  SNR %.1f", rssi_, static_cast<double>(snr_));
  } else if (haveLast_) {
    display_.printf("Ultimo %s", rxCall_[0] ? rxCall_ : "-");
    display_.setCursor(0, 47);  // 1 pixel di stacco dalla riga sopra e da "Vol"
    display_.printf("persi %lu  recup. %lu", static_cast<unsigned long>(lastLost_),
                    static_cast<unsigned long>(lastRecovered_));
  } else {
    display_.print(message_ ? "PTT = registra" : (testMode_ ? "PTT = invia la prova" : "Premi PTT per parlare"));
  }

  display_.setCursor(0, 56);
  display_.printf("Vol %d/10", volume_);

  // Batteria in basso a destra, nel formato di Guzman: "(Batt=4.12V)". Se e' scarica il testo va in
  // negativo (sfondo pieno): la larghezza resta la stessa (12 caratteri = 72 pixel, a sinistra c'e' "Vol").
  char battText[20];
  const size_t len = lcr::formatBatteryText(battText, sizeof(battText), batteryShown_);
  const int battX = 128 - static_cast<int>(len) * 6;
  if (lcr::isLowBattery(batteryShown_)) {
    display_.fillRect(battX - 1, 55, 128 - battX + 1, 9, SSD1306_WHITE);
    display_.setTextColor(SSD1306_BLACK);
  }
  display_.setCursor(battX, 56);
  display_.print(battText);
  display_.setTextColor(SSD1306_WHITE);
  display_.display();
}
