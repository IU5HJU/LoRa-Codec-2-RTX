// Interfaccia utente: display OLED SSD1306 e segnali acustici.
// Implementa lcr::INotifier. Solo per ESP32 (Arduino): non e' compilato nei test sul PC.
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include "audio_io.h"
#include "choice_picker.h"
#include "inbox.h"
#include "interfaces.h"
#include "menu.h"
#include "morse.h"
#include "settings.h"
#include "text_picker.h"
#include "voice_node.h"

class UiNotifier : public lcr::INotifier {
 public:
  explicit UiNotifier(AudioIo& audio) : audio_(audio), display_(128, 64, &Wire, -1) {}

  // Nominativo e frequenza mostrati sul display (da chiamare appena lette le impostazioni).
  void setSettings(const lcr::Settings& s) { settings_ = s; }
  // Profilo in uso: cambia i testi (messaggio) e il limite di durata mostrato.
  void setProfile(const lcr::Profile& p) {
    message_ = p.message;
    badge_ = p.id == lcr::kProfileMessage ? 'M' : (p.id == lcr::kProfileQuality ? 'Q' : 'S');
    maxTurnSec_ = p.maxTurnMs / 1000;
  }

  // Posizione di questa stazione (QTH fisso o GPS): serve a mostrare la distanza dalla stazione ricevuta.
  void setOwnPosition(const lcr::Position& p) { ownPos_ = p; }

  // Stato del GPS (T-Beam): spento/acceso, fix si/no e satelliti; compare nella schermata a riposo.
  void setGpsStatus(bool enabled, bool fix, int sats) {
    if (enabled != gpsEnabled_ || fix != gpsFix_ || sats != gpsSats_) dirty_ = true;
    gpsEnabled_ = enabled;
    gpsFix_ = fix;
    gpsSats_ = sats;
  }

  // Menu delle modalita': mentre e' aperto lo schermo mostra le voci. currentId e' il profilo in uso (segnato con *).
  void setMenu(const lcr::Menu* menu, uint8_t currentId) {
    menu_ = menu;
    menuCurrentId_ = currentId;
  }

  // Casella dei messaggi (testo da mostrare) e scelta del testo da inviare.
  void setInbox(const lcr::Inbox* inbox) { inbox_ = inbox; }
  void setPicker(const lcr::TextPicker* picker) { picker_ = picker; }
  // Scelta del testo e del destinatario dell'escursione MeshCom (titolo sulla prima riga).
  void setChoicePicker(const lcr::ChoicePicker* picker) { choice_ = picker; }
  void setChoiceTitle(const char* title) {
    strncpy(choiceTitle_, title, sizeof(choiceTitle_) - 1);
    choiceTitle_[sizeof(choiceTitle_) - 1] = 0;
  }
  // Secondi che restano all'escursione MeshCom (0 = non in corso): cambia la lettera del profilo (T), la frequenza e lo schermo.
  void setMeshcomRemaining(int sec) {
    if (sec != meshSec_) dirty_ = true;
    meshSec_ = sec;
  }
  // Pagina successiva del testo aperto (volume breve mentre si legge un testo lungo). false se il testo ha una sola pagina.
  bool nextTextPage();
  bool textOpen() const { return openedText_; }
  // Prima di node->sendText(): lo schermo di trasmissione dice "INVIO SMS" invece del tempo di parlato.
  void setSendingText(bool on) { sendingText_ = on; }

  // false se il display non risponde (il resto del sistema funziona comunque).
  bool begin();
  void showMessage(const char* line1, const char* line2 = nullptr);  // schermata bloccante (avvio/errori)
  void showWifi(const char* ssid, const char* password);  // configurazione Wi-Fi: password in carattere doppio
  void showLines(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr, const char* l4 = nullptr,
                 const char* l5 = nullptr);  // fino a 5 righe di testo piccolo

  // Da chiamare nel loop: ridisegna a ~4 Hz e quando cambia qualcosa.
  // batteryVolts: tensione della batteria in volt, negativa se non disponibile.
  void loop(uint32_t nowMs, lcr::VoiceNode::State state, int volumeStep, int rssiDbm, float snrDb,
            float batteryVolts);

  // lcr::INotifier
  void txStarted() override;
  void txEnded(bool turnLimitReached) override;
  void pttDenied(lcr::DenyReason r) override;
  void rxStarted() override;
  void rxCallsign(const char* callsign) override;
  void rxPosition(const lcr::Position& p) override;
  void rxPlaybackStarted() override {
    playing_ = true;
    dirty_ = true;
  }
  void txProgress(int percent) override {
    txPct_ = percent;
    dirty_ = true;
  }
  void rxProgress(int percent) override {
    rxPct_ = percent;
    dirty_ = true;
  }
  void rxEnded(const char* callsign, uint32_t lostFrames, uint32_t recoveredPackets) override;
  void testModeChanged(bool on) override;
  void messageReceived(bool isText, const char* callsign) override;
  void messageOpened(bool isText) override;
  void messageClosed() override;
  void meshcomChanged(bool on) override;
  void recPhaseChanged(int phase) override { recPhase_ = phase; dirty_ = true; }
  void linkTestReport(bool fromRemote, const char* callsign, const lcr::TestReport& r) override;

  // Forza il ridisegno dello schermo (dopo una schermata temporanea).
  void refresh() { dirty_ = true; }

  // Prova di collegamento attiva: lo schermo a riposo lo dice.
  void setTestMode(bool on) {
    if (on != testMode_) dirty_ = true;
    testMode_ = on;
  }

 private:
  void draw(uint32_t nowMs);
  void drawTestReport();
  void drawMenu();
  void drawProfileBadge();
  void drawIncoming();
  void drawPlaying();
  void drawText();
  void drawPicker();
  void drawChoice();
  void drawEnvelope(int x, int y);
  void drawSpeaker(int x, int y);
  void wrapText(const char* text);

  AudioIo& audio_;
  lcr::Settings settings_;
  Adafruit_SSD1306 display_;
  bool ok_ = false;
  bool dirty_ = true;
  uint32_t lastDrawMs_ = 0;

  bool gpsEnabled_ = false;
  bool gpsFix_ = false;
  int gpsSats_ = 0;
  lcr::Position ownPos_;
  char distText_[12] = {0};  // distanza dall'ultima stazione ricevuta ("12.3km"), vuota se non nota
  const lcr::Menu* menu_ = nullptr;
  uint8_t menuCurrentId_ = 0;
  bool message_ = false;         // profilo Messaggio
  const lcr::ChoicePicker* choice_ = nullptr;
  char choiceTitle_[22] = {0};
  int meshSec_ = 0;              // secondi che restano all'escursione MeshCom
  char badge_ = 'S';             // lettera del profilo in uso (S Standard, M Messaggio, Q Qualita'), nel riquadro in alto a destra
  uint32_t maxTurnSec_ = 30;     // durata massima di un turno o di un messaggio, secondi
  int txPct_ = 0;                // avanzamento dell'invio di un messaggio, 1-100 (0 = non noto)
  int rxPct_ = 0;                // avanzamento della ricezione di un messaggio, 1-100 (0 = non noto)
  bool playing_ = false;         // profilo Messaggio: sta riproducendo (altrimenti sta ancora ricevendo)
  char noticeBuf_[16] = {0};
  int recPhase_ = 0;  // registra e poi trasmetti: 0 nessuna, 1 registra, 2 attesa, 3 trasmette
  lcr::VoiceNode::State state_ = lcr::VoiceNode::State::Idle;
  int volume_ = 0;
  int rssi_ = 0;
  float snr_ = 0;
  float battery_ = -1.0f;        // ultima misura
  float batteryShown_ = -1.0f;   // valore a schermo (con isteresi, per non ridisegnare di continuo)
  uint32_t batteryShownMs_ = 0;
  uint32_t txStartMs_ = 0;
  bool limitHit_ = false;
  char rxCall_[lcr::kMaxCallsign + 1] = {0};
  uint32_t lastLost_ = 0;
  uint32_t lastRecovered_ = 0;
  bool haveLast_ = false;
  bool testMode_ = false;
  bool testShown_ = false;        // schermata con l'esito della prova
  uint32_t testUntilMs_ = 0;
  bool testFromRemote_ = false;
  char testCall_[lcr::kMaxCallsign + 1] = {0};
  lcr::TestReport testRep_;
  const char* notice_ = nullptr;
  // Messaggi: un solo messaggio in casella (vedi inbox.h)
  const lcr::Inbox* inbox_ = nullptr;
  const lcr::TextPicker* picker_ = nullptr;
  bool openedText_ = false;       // testo aperto e in mostra
  bool sendingText_ = false;
  bool openedVoice_ = false;      // messaggio vocale in riproduzione
  int textPage_ = 0;
  int textLineCount_ = 0;
  char textLines_[16][22];        // testo a capo, 21 caratteri per riga
  uint32_t noticeUntilMs_ = 0;
};
