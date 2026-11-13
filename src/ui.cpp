#include "ui.h"

#include "settings_nvs.h"

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

// Lettera del profilo in uso (S Standard, R Range, M Messaggio) in alto a destra: bianca su sfondo nero (in positivo),
// disegnata a mano su 10x9 punti con tratti di 2 punti per occupare tutto lo spazio sopra la riga e restare leggibile.
void UiNotifier::drawProfileBadge() {
  static const char* const kS[9] = {".XXXXXXXX.", "XXXXXXXXXX", "XX........", "XXXXXXXX..", ".XXXXXXXX.",
                                    "..XXXXXXXX", "........XX", "XXXXXXXXXX", ".XXXXXXXX."};
  static const char* const kM[9] = {"XX......XX", "XXX....XXX", "XXXX..XXXX", "XX.XXXX.XX", "XX..XX..XX",
                                    "XX......XX", "XX......XX", "XX......XX", "XX......XX"};
  static const char* const kR[9] = {"XXXXXXXX..", "XXXXXXXXX.", "XX.....XXX", "XX.....XXX", "XXXXXXXXX.",
                                    "XXXXXXXX..", "XX..XXX...", "XX...XXX..", "XX....XXXX"};
  static const char* const kT[9] = {"XXXXXXXXXX", "XXXXXXXXXX", "....XX....", "....XX....", "....XX....",
                                    "....XX....", "....XX....", "....XX....", "....XX...."};
  const char b = meshSec_ > 0 ? 'T' : badge_;  // durante l'escursione MeshCom la lettera e' T
  const char* const* g = b == 'M' ? kM : (b == 'R' ? kR : (b == 'T' ? kT : kS));
  for (int y = 0; y < 9; y++)
    for (int x = 0; x < 10; x++)
      if (g[y][x] == 'X') display_.drawPixel(118 + x, y, SSD1306_WHITE);
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

// Schermata a righe di testo piccolo (fino a 5 righe da 21 caratteri), per la configurazione Wi-Fi.
void UiNotifier::showLines(const char* l1, const char* l2, const char* l3, const char* l4, const char* l5) {
  if (!ok_) return;
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  const char* lines[5] = {l1, l2, l3, l4, l5};
  for (int i = 0; i < 5; i++) {
    if (!lines[i]) continue;
    display_.setCursor(0, i * 12);
    display_.println(lines[i]);
  }
  display_.display();
}

// Schermata della configurazione Wi-Fi: la password e' in carattere doppio (e in grassetto) per leggerla senza occhiali.
void UiNotifier::showWifi(const char* ssid, const char* password) {
  if (!ok_) return;
  display_.clearDisplay();
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.printf("WiFi %s", ssid);
  display_.setCursor(0, 12);
  display_.print("Password:");
  display_.setTextSize(2);  // 12x16 punti per carattere: 8 cifre = 96 punti
  for (int dx = 0; dx < 2; dx++) {  // stampata due volte con 1 punto di scarto: tratto piu' spesso
    display_.setCursor(dx, 24);
    display_.print(password);
  }
  display_.setTextSize(1);
  display_.setCursor(0, 46);
  display_.print("http://192.168.4.1");
  display_.setCursor(0, 56);
  display_.print("VOL lungo = esci");
  display_.display();
}

void UiNotifier::txStarted() {
  txPct_ = 0;
  txStartMs_ = millis();
  limitHit_ = false;
  haveLast_ = false;
  dirty_ = true;
}

void UiNotifier::txEnded(bool turnLimitReached) {
  sendingText_ = false;
  sendingCmd_ = false;
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
    case lcr::DenyReason::TextOnly: notice_ = "SOLO TESTO"; break;
    case lcr::DenyReason::NoDestination: notice_ = "IMPOSTA DESTINAT."; break;
  }
  noticeUntilMs_ = millis() + 1500;
  dirty_ = true;
  audio_.beep(300, 150);
}

void UiNotifier::rxStarted() {
  viaRpt_ = false;
  rxPct_ = 0;
  rxCall_[0] = 0;
  distText_[0] = 0;
  playing_ = false;
  haveLast_ = false;
  dirty_ = true;
  if (message_) {
    // Nel profilo Messaggio la ricezione dura minuti: all'inizio suona "MSG" in Morse (come lo squillo degli SMS dei
    // vecchi telefoni) per attirare l'attenzione. Prima si mostra lo schermo di ricezione, poi si suona (circa 1,6 s,
    // bloccante: il pacchetto successivo del messaggio arriva dopo almeno 3 s).
    state_ = lcr::VoiceNode::State::Receiving;
    draw(millis());
    lastDrawMs_ = millis();
    dirty_ = false;
    audio_.playMorse(lcr::kMessageTone, lcr::kMessageToneHz, lcr::kMessageToneUnitMs);
  }
}

void UiNotifier::rxCallsign(const char* callsign) {
  strncpy(rxCall_, callsign, lcr::kMaxCallsign);
  rxCall_[lcr::kMaxCallsign] = 0;
  dirty_ = true;
}

// Identificativo del ripetitore: diventa l'ultimo ricevuto (con segnale e qualita'), senza bip e senza interrompere nulla.
void UiNotifier::beaconReceived(const char* callsign) {
  strncpy(rxCall_, callsign, lcr::kMaxCallsign);
  rxCall_[lcr::kMaxCallsign] = 0;
  distText_[0] = 0;
  haveLast_ = true;
  lastVia_ = false;
  lastBeacon_ = true;
  Serial.printf("RX identificativo: %s RSSI %d dBm SNR %.1f dB\n", rxCall_, rssi_, static_cast<double>(snr_));
  dirty_ = true;
}

void UiNotifier::rxPosition(const lcr::Position& p) {
  if (!ownPos_.valid) return;  // senza la propria posizione non si calcola nessuna distanza
  lcr::formatDistance(distText_, sizeof(distText_), lcr::distanceKm(ownPos_, p));
  dirty_ = true;
}

void UiNotifier::rxEnded(const char* callsign, uint32_t lostFrames, uint32_t recoveredPackets) {
  lastVia_ = viaRpt_;
  lastBeacon_ = false;
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
  if (sweepRunning_) {  // nella prova di potenza il passo e' segnalato da powerSweepStep, senza una schermata e un bip per ogni risposta
    Serial.printf("PROVA risposta da %s: ok %u/%u, RSSI %d dBm, SNR %s%d dB\n", callsign && callsign[0] ? callsign : "-", r.framesOk, static_cast<unsigned>(lcr::kSweepFrames),
                  r.rssiDbm, r.snrValid ? "" : "n.d. ", r.snrValid ? r.snrDb : 0);
    return;
  }
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

// Messaggio arrivato in casella: icona e invito a premere il PTT. Il testo suona "SMS" in Morse (il vocale ha gia' suonato
// "MSG" all'inizio della ricezione).
void UiNotifier::messageReceived(bool isText, const char* callsign) {
  (void)callsign;
  openedText_ = openedVoice_ = false;
  dirty_ = true;
  if (isText) {
    state_ = lcr::VoiceNode::State::Idle;
    draw(millis());  // prima l'icona, poi il suono (bloccante, circa 2 s)
    lastDrawMs_ = millis();
    dirty_ = false;
    audio_.playMorse(lcr::kTextTone, lcr::kMessageToneHz, lcr::kMessageToneUnitMs);
  }
}

void UiNotifier::messageOpened(bool isText) {
  openedText_ = isText;
  openedVoice_ = !isText;
  textPage_ = 0;
  if (isText && inbox_) wrapText(inbox_->text());
  dirty_ = true;
}

void UiNotifier::messageClosed() {
  openedText_ = openedVoice_ = false;
  dirty_ = true;
}

bool UiNotifier::nextTextPage() {
  if (!openedText_) return false;
  const int pages = (textLineCount_ + 5) / 6;
  if (pages <= 1) return false;
  textPage_ = (textPage_ + 1) % pages;
  dirty_ = true;
  return true;
}

// Testo a capo per parole su righe di 21 caratteri (una parola piu' lunga si spezza).
void UiNotifier::wrapText(const char* text) {
  textLineCount_ = 0;
  const size_t n = strlen(text);
  size_t i = 0;
  while (i < n && textLineCount_ < 16) {
    size_t len = n - i < 21 ? n - i : 21;
    if (n - i > 21) {  // dove spezzare: all'ultimo spazio entro la riga, altrimenti a 21
      size_t cut = 0;
      for (size_t k = 0; k <= 21 && i + k < n; k++) {
        if (text[i + k] == ' ') cut = k;
      }
      if (cut > 0) len = cut;
    }
    memcpy(textLines_[textLineCount_], text + i, len);
    textLines_[textLineCount_][len] = 0;
    textLineCount_++;
    i += len;
    while (i < n && text[i] == ' ') i++;  // lo spazio dove si e' andati a capo non si mostra
  }
}

// Busta: rettangolo con la linguetta a V (44x28 punti).
void UiNotifier::drawEnvelope(int x, int y) {
  display_.drawRect(x, y, 44, 28, SSD1306_WHITE);
  display_.drawRect(x + 1, y + 1, 42, 26, SSD1306_WHITE);
  for (int k = 0; k < 2; k++) {
    display_.drawLine(x + 1, y + 1 + k, x + 22, y + 15 + k, SSD1306_WHITE);
    display_.drawLine(x + 42, y + 1 + k, x + 22, y + 15 + k, SSD1306_WHITE);
  }
}

// Altoparlante con le onde (44x32 punti).
void UiNotifier::drawSpeaker(int x, int y) {
  display_.fillRect(x, y + 10, 10, 12, SSD1306_WHITE);
  display_.fillTriangle(x + 10, y + 10, x + 24, y, x + 24, y + 31, SSD1306_WHITE);
  display_.fillTriangle(x + 10, y + 10, x + 10, y + 21, x + 24, y + 31, SSD1306_WHITE);
  display_.drawCircleHelper(x + 26, y + 16, 8, 0x6, SSD1306_WHITE);
  display_.drawCircleHelper(x + 26, y + 16, 15, 0x6, SSD1306_WHITE);
}

// Messaggio in casella, non ancora aperto: grande icona (altoparlante = voce, busta = testo) e come aprirlo.
void UiNotifier::drawIncoming() {
  const bool text = inbox_->kind() == lcr::Inbox::Kind::Text;
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.print(text ? "SMS da " : "Voce da ");
  display_.print(inbox_->callsign());
  if (text) drawEnvelope(42, 11); else drawSpeaker(40, 10);
  display_.setCursor(0, 44);
  display_.print("MESSAGGIO RICEVUTO");  // 18 caratteri
  display_.setCursor(0, 55);
  display_.print(text ? "Premi PTT: leggi" : "Premi PTT: ascolta");
  display_.display();
}

void UiNotifier::drawPlaying() {
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.print("Voce da ");
  display_.print(inbox_->callsign());
  drawSpeaker(40, 10);
  display_.setCursor(0, 44);
  display_.print("Riproduco messaggio");
  display_.setCursor(0, 55);
  display_.print("PTT = stop");
  display_.display();
}

// Testo aperto: titolo, righe a capo (6 per pagina); con piu' pagine il volume (pressione breve) passa alla successiva.
void UiNotifier::drawText() {
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  const int pages = (textLineCount_ + 5) / 6;
  if (pages > 1) display_.printf("da %s  %d/%d  VOL>", inbox_->callsign(), textPage_ + 1, pages);
  else display_.printf("da %s", inbox_->callsign());
  display_.drawFastHLine(0, 9, 128, SSD1306_WHITE);
  for (int r = 0; r < 6; r++) {
    const int li = textPage_ * 6 + r;
    if (li >= textLineCount_) break;
    display_.setCursor(0, 12 + r * 8);
    display_.print(textLines_[li]);
  }
  display_.display();
}

// Scelta del testo da inviare: numero, testo a capo (come drawText) e istruzioni.
void UiNotifier::drawPicker() {
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.printf("INVIA TESTO %u/%u", static_cast<unsigned>(picker_->ordinal()), static_cast<unsigned>(picker_->usedCount()));
  display_.drawFastHLine(0, 9, 128, SSD1306_WHITE);
  wrapText(picker_->text());
  for (int r = 0; r < 5 && r < textLineCount_; r++) {
    display_.setCursor(0, 12 + r * 8);
    display_.print(textLines_[r]);
  }
  display_.setCursor(0, 55);
  display_.print("PTT=altro  VOL=invia");
  display_.display();
}

// Risposta firmata di un ripetitore a un comando del sysop (CFG SYSCMD): una riga sulla seriale e un bip.
void UiNotifier::sysopReply(const char* callsign, const lcr::SysopReply& r) {
  Serial.printf("CFG SYSCMD RISPOSTA da=%s codice=%u esito=%u contatore=%lu spento=%u potenza=%u minuti=%u\n", callsign, static_cast<unsigned>(r.code),
                static_cast<unsigned>(r.result), static_cast<unsigned long>(r.counter), static_cast<unsigned>(r.data >> 24),
                static_cast<unsigned>((r.data >> 16) & 0xFF), static_cast<unsigned>(r.data & 0xFFFF));
  if (r.result == lcr::kSysopCounterLow) {
    saveSysopCounter(r.counter);  // il ripetitore dice l'ultimo contatore accettato: il prossimo comando passa
    snprintf(noticeBuf_, sizeof(noticeBuf_), "RIPETI IL COMANDO");
  } else if (r.result != lcr::kSysopOk) {
    snprintf(noticeBuf_, sizeof(noticeBuf_), "RIP: RIFIUTATO %u", static_cast<unsigned>(r.result));
  } else if (r.code == lcr::kSysopStatus || r.code == lcr::kSysopOff || r.code == lcr::kSysopOffMinutes || r.code == lcr::kSysopOn) {
    const unsigned mins = r.data & 0xFFFF;
    if ((r.data >> 24) == 0) snprintf(noticeBuf_, sizeof(noticeBuf_), "RIP: ATTIVO %udBm", static_cast<unsigned>((r.data >> 16) & 0xFF));
    else if (mins == 0xFFFF) snprintf(noticeBuf_, sizeof(noticeBuf_), "RIP: SPENTO");
    else snprintf(noticeBuf_, sizeof(noticeBuf_), "RIP: SPENTO %umin", mins);
  } else {
    snprintf(noticeBuf_, sizeof(noticeBuf_), "RIP: OK");
  }
  notice_ = noticeBuf_;
  noticeUntilMs_ = millis() + 4000;
  audio_.beep(r.result == lcr::kSysopOk ? 1000 : 400, 80);
  dirty_ = true;
}

// Inizio e fine dell'escursione sulla rete di testo: un bip diverso e il ridisegno.
void UiNotifier::meshcomChanged(bool on) {
  dirty_ = true;
  if (on) {
    audio_.beep(900, 50);
    audio_.beep(1200, 50);
  } else {
    audio_.beep(1200, 50);
    audio_.beep(700, 80);
  }
}

// Il ripetitore dice che e' occupato: un bip, poi la riga compare a riposo (il PTT resta libero).
void UiNotifier::repeaterBusy(const char* callsign, uint16_t seconds) {
  (void)callsign;
  dirty_ = true;
  if (seconds > 0) audio_.beep(500, 80);
}

// Scelta del testo o del destinatario dell'escursione MeshCom: titolo, posizione, voce a capo e istruzioni.
void UiNotifier::drawChoice() {
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 0);
  display_.printf("%s %u/%u", choiceTitle_, static_cast<unsigned>(choice_->index() + 1), static_cast<unsigned>(choice_->count()));
  display_.drawFastHLine(0, 9, 128, SSD1306_WHITE);
  wrapText(choice_->text());
  for (int r = 0; r < 5 && r < textLineCount_; r++) {
    display_.setCursor(0, 12 + r * 8);
    display_.print(textLines_[r]);
  }
  display_.setCursor(0, 55);
  display_.print("PTT=altro  VOL=scegli");
  display_.display();
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

// Un passo della prova di potenza: riga per il registro (docs/prove.md) e schermo.
void UiNotifier::powerSweepStep(const lcr::PowerSweep::Step& s, size_t index) {
  if (s.answered) {
    Serial.printf("PROVA POTENZA %d dBm: risposta, frame ok %u%%, RSSI %d dBm, SNR %s%d dB\n", s.dbm, s.percent, s.rssiDbm, s.snrValid ? "" : "n.d. ", s.snrValid ? s.snrDb : 0);
  } else {
    Serial.printf("PROVA POTENZA %d dBm: nessuna risposta\n", s.dbm);
  }
  sweepDbm_ = s.dbm;
  sweepPct_ = s.answered ? s.percent : 0;
  sweepIndex_ = static_cast<int>(index) + 1;
  dirty_ = true;
}

void UiNotifier::powerSweepDone(const lcr::PowerSweep& sweep, bool aborted) {
  sweepRunning_ = false;
  sweepFromRemote_ = false;
  sweepAborted_ = aborted;
  sweepMinPerfect_ = sweep.minPerfectDbm();
  sweepMinAnswered_ = sweep.minAnsweredDbm();
  sweepShown_ = true;
  sweepUntilMs_ = millis() + 30000;
  dirty_ = true;
  char perfect[24], answered[24];
  if (sweepMinPerfect_ >= 0) snprintf(perfect, sizeof(perfect), "%d dBm", sweepMinPerfect_);
  else snprintf(perfect, sizeof(perfect), "nessuna potenza");
  if (sweepMinAnswered_ >= 0) snprintf(answered, sizeof(answered), "%d dBm", sweepMinAnswered_);
  else snprintf(answered, sizeof(answered), "mai");
  Serial.printf("PROVA POTENZA %s: %u passi; frame tutti intatti fino a: %s; risposta ancora a: %s; potenza rimessa a %d dBm\n", aborted ? "INTERROTTA" : "finita",
                static_cast<unsigned>(sweep.steps()), perfect, answered, sweep.restoreDbm());
  if (!aborted && sweepMinPerfect_ >= 0) {
    audio_.beep(1000, 60);
    audio_.beep(1400, 60);
  } else {
    audio_.beep(300, 200);
  }
}

// L'esito della prova di potenza visto dall'altra scheda, che ha trasmesso: lo mostriamo anche qui.
void UiNotifier::powerSweepSummary(const char* callsign, const lcr::SweepSummary& s) {
  sweepRunning_ = false;
  sweepFromRemote_ = true;
  sweepAborted_ = false;
  sweepMinPerfect_ = s.minPerfectDbm;
  sweepMinAnswered_ = s.minAnsweredDbm;
  sweepSteps_ = s.n;
  strncpy(sweepCall_, callsign ? callsign : "", lcr::kMaxCallsign);
  sweepCall_[lcr::kMaxCallsign] = 0;
  sweepShown_ = true;
  sweepUntilMs_ = millis() + 30000;
  dirty_ = true;
  testShown_ = false;
  Serial.printf("PROVA POTENZA esito da %s: %u passi dal massimo %d dBm; frame tutti intatti fino a %d dBm%s; risposta ancora a %d dBm%s\n", sweepCall_[0] ? sweepCall_ : "-",
                s.n, s.firstDbm, s.minPerfectDbm, s.minPerfectDbm < 0 ? " (nessuna)" : "", s.minAnsweredDbm, s.minAnsweredDbm < 0 ? " (mai)" : "");
  if (s.minPerfectDbm >= 0) {
    audio_.beep(1000, 60);
    audio_.beep(1400, 60);
  } else {
    audio_.beep(300, 200);
  }
}

void UiNotifier::drawSweep() {
  display_.setTextSize(1);
  display_.setCursor(0, 16);
  if (sweepRunning_) display_.print("PROVA DI POTENZA");
  else if (sweepFromRemote_) display_.printf("POTENZA da %s", sweepCall_[0] ? sweepCall_ : "-");
  else display_.print(sweepAborted_ ? "POTENZA: interrotta" : "POTENZA: finita");
  display_.setTextSize(2);
  display_.setCursor(0, 28);
  if (sweepRunning_) {
    display_.printf("%d dBm", sweepDbm_);
  } else if (sweepMinPerfect_ >= 0) {
    display_.printf("MIN %d dBm", sweepMinPerfect_);
  } else {
    display_.print(sweepMinAnswered_ >= 0 ? "NON PULITA" : "NESSUNA");
  }
  display_.setTextSize(1);
  display_.setCursor(0, 47);
  if (sweepRunning_) {
    if (sweepPct_ < 0) display_.print("passo 1...");
    else display_.printf("passo %d: %s", sweepIndex_, sweepPct_ > 0 ? "risposta" : "nessuna risp.");
    display_.setCursor(0, 56);
    display_.print("PTT = ferma");
  } else {
    display_.printf("sente fino a %d dBm", sweepMinAnswered_);  // max 21 caratteri
    if (sweepMinAnswered_ < 0) {
      display_.setCursor(0, 47);
      display_.print("nessuna risposta");
    }
    display_.setCursor(0, 56);
    display_.print("(frame 100% a MIN)");
  }
  display_.display();
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
    display_.printf("Parz.%u/%u %s", r.framesOut, static_cast<unsigned>(lcr::kSweepFrames), testCall_[0] ? testCall_ : "-");
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

// Menu delle modalita': una voce per riga (la scelta in negativo, la modalita' in uso con *, le voci non
// disponibili con "presto"), istruzioni in basso. Al massimo 21 caratteri per riga a testo piccolo.
void UiNotifier::drawMenu() {
  display_.setTextSize(1);
  // Cinque righe da 9 punti: niente titolo, per lasciare posto alle istruzioni in basso (y 57). Con piu' voci la finestra
  // scorre e la voce scelta resta sempre visibile.
  constexpr size_t kRows = 5;
  const size_t first = menu_->selectedIndex() >= kRows ? menu_->selectedIndex() - (kRows - 1) : 0;
  for (size_t i = first; i < menu_->count() && i < first + kRows; i++) {
    const lcr::MenuItem& it = menu_->item(i);
    const int y = 12 + static_cast<int>(i - first) * 9;
    const bool sel = i == menu_->selectedIndex();
    if (sel) {
      display_.fillRect(0, y - 1, 128, 9, SSD1306_WHITE);
      display_.setTextColor(SSD1306_BLACK);
    }
    display_.setCursor(2, y);
    display_.printf("%c %s%s", it.id == menuCurrentId_ ? '*' : ' ', it.label, it.enabled ? "" : " (presto)");
    display_.setTextColor(SSD1306_WHITE);
  }
  display_.setCursor(0, 57);
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
  if (sweepShown_ && !sweepRunning_ && nowMs > sweepUntilMs_) {
    sweepShown_ = false;
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
  const double shownMhz = meshSec_ > 0 ? settings_.freqMcKhz / 1000.0 : static_cast<double>(settings_.freqMhz());
  display_.printf("%s %.3fMHz", settings_.callsign, shownMhz);
  drawProfileBadge();
  display_.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  if (picker_ && picker_->isOpen()) {
    drawPicker();
    return;
  }
  if (choice_ && choice_->isOpen()) {
    drawChoice();
    return;
  }

  if (menu_ && menu_->isOpen()) {
    drawMenu();
    return;
  }

  if (state_ == lcr::VoiceNode::State::Idle && inbox_ && inbox_->has()) {  // messaggio in casella
    if (openedText_) drawText();
    else if (openedVoice_) drawPlaying();
    else drawIncoming();
    return;
  }

  if (sweepShown_ && (sweepRunning_ || state_ == lcr::VoiceNode::State::Idle)) {
    drawSweep();
    return;
  }
  if (testShown_ && state_ == lcr::VoiceNode::State::Idle) {
    drawTestReport();
    return;
  }

  char big[24];
  switch (state_) {
    case lcr::VoiceNode::State::Idle: snprintf(big, sizeof(big), "%s", meshSec_ > 0 ? "MESHCOM" : (message_ ? "MESSAGGIO" : (testMode_ ? "PROVA ON" : "ASCOLTO"))); break;
    case lcr::VoiceNode::State::Receiving: snprintf(big, sizeof(big), "RX %s", rxCall_); break;
    default:
      if (sendingText_) snprintf(big, sizeof(big), sendingCmd_ ? "COMANDO" : "INVIO SMS");
      else if (recPhase_ == 1) snprintf(big, sizeof(big), "REGISTRA");
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
  } else if (state_ == lcr::VoiceNode::State::Transmitting && sendingText_) {
    display_.print(sendingCmd_ ? "Trasmetto il comando" : "Trasmetto il testo");
  } else if (state_ == lcr::VoiceNode::State::Transmitting && recPhase_ == 1) {
    display_.printf("Registro %lu/%lu s", static_cast<unsigned long>((nowMs - txStartMs_) / 1000),
                    static_cast<unsigned long>(maxTurnSec_));
  } else if (state_ == lcr::VoiceNode::State::Transmitting && recPhase_ == 2) {
    display_.print("Parte tra 1 secondo");
  } else if (state_ == lcr::VoiceNode::State::Transmitting && recPhase_ == 3) {
    if (txPct_ > 0) {
      display_.printf("Invio messaggio %d%%", txPct_);
      display_.drawRect(0, 47, 128, 7, SSD1306_WHITE);  // barra di avanzamento sulla riga libera
      display_.fillRect(2, 49, (124 * txPct_) / 100, 3, SSD1306_WHITE);
    } else {
      display_.print("Trasmetto messaggio");
    }
  } else if (state_ == lcr::VoiceNode::State::Transmitting) {
    display_.printf(testMode_ ? "Prova %lu/12 s" : "Trasmissione %lu/%lu s",
                    static_cast<unsigned long>((nowMs - txStartMs_) / 1000), static_cast<unsigned long>(maxTurnSec_));
  } else if (state_ == lcr::VoiceNode::State::Receiving && message_) {
    if (playing_) {
      display_.print("Riproduco messaggio");
    } else if (rxPct_ > 0) {
      display_.printf("Ricevo messaggio %d%%", rxPct_);  // al massimo 21 caratteri
    } else {
      display_.print("Ricevo il messaggio");
    }
    display_.setCursor(0, 47);
    if (distText_[0]) {
      display_.printf("%s  RSSI %d", distText_, rssi_);
    } else {
      display_.printf("RSSI %d  SNR %.1f", rssi_, static_cast<double>(snr_));
    }
  } else if (state_ == lcr::VoiceNode::State::Receiving) {
    display_.printf("RSSI %d  SNR %.1f", rssi_, static_cast<double>(snr_));
    if (distText_[0]) {
      display_.setCursor(0, 47);
      display_.printf("Distanza %s", distText_);
    }
  } else if (haveLast_) {
    {  // "Ultimo IU5HJU 12.3km": la distanza si aggiunge solo se la riga resta entro i 21 caratteri
      char line[32];
      // "Ultimo IZ5FSN via RPT" (arrivata da un ripetitore) o "Ultimo IR5XXX ID" (identificativo del ripetitore)
      int n = snprintf(line, sizeof(line), "Ultimo %s%s", rxCall_[0] ? rxCall_ : "-", lastBeacon_ ? " ID" : (lastVia_ ? " via RPT" : ""));
      if (distText_[0] && n > 0 && n + 1 + static_cast<int>(strlen(distText_)) <= 21) snprintf(line + n, sizeof(line) - n, " %s", distText_);
      display_.print(line);
    }
    display_.setCursor(0, 47);  // 1 pixel di stacco dalla riga sopra e da "Vol"
    if (lastBeacon_) display_.printf("RSSI %d  SNR %.1f", rssi_, static_cast<double>(snr_));
    else display_.printf("persi %lu  recup. %lu", static_cast<unsigned long>(lastLost_), static_cast<unsigned long>(lastRecovered_));
  } else {
    if (meshSec_ > 0) {
      display_.printf("Ascolto %d:%02d", meshSec_ / 60, meshSec_ % 60);
      display_.setCursor(0, 47);
      display_.print("VOL lungo = esci");  // torna subito alla voce
    } else display_.print(message_ ? "PTT = registra" : (testMode_ ? "PTT = invia la prova" : "Premi PTT per parlare"));
    if (busySec_ > 0 && meshSec_ <= 0) {  // un ripetitore sta ritrasmettendo un messaggio: solo un avviso
      display_.setCursor(0, 47);
      display_.printf("RIP. OCCUPATO %d:%02d", busySec_ / 60, busySec_ % 60);
    } else if (gpsEnabled_ && meshSec_ <= 0) {  // riga libera a riposo: stato del GPS (durante l'escursione c'e' il promemoria)
      display_.setCursor(0, 47);
      if (gpsFix_) {
        display_.printf("GPS fix, %d satelliti", gpsSats_);
      } else {
        display_.print("GPS cerco satelliti");
      }
    }
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
